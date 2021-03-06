/*

   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*****************************************************************************/
/*                                                                           */
/* File: cfreport.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepReportsControlPromises(void);
void KeepReportsPromises(void);
void ShowLastSeen(void);
void ShowPerformance(void);
void ShowLastSeen(void);
void ShowClasses(void);
void ShowChecksums(void);
void ShowLocks (int active);
void ShowCurrentAudit(void);
char *Format(char *s,int width);
int CompareClasses(const void *a, const void *b);
void ReadAverages(void);
void SummarizeAverages(void);
void WriteGraphFiles(void);
void WriteHistograms(void);
void DiskArrivals(void);
void PeerIntermittency(void);
void GetFQHN(void);
void OpenFiles(void);
void CloseFiles(void);
void MagnifyNow(void);
void OpenMagnifyFiles(void);
void CloseMagnifyFiles(void);
void EraseAverages(void);
void RemoveHostSeen(char *hosts);

extern struct BodySyntax CFRE_CONTROLBODY[];

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFRP_CONTROLBODY[];

int HTML = false;
int GRAPH = false;
int TITLES = false;
int TIMESTAMPS = false;
int HIRES = false;
int ERRORBARS = true;
int NOSCALING = true;
int NOWOPT = false;
int EMBEDDED = false;

unsigned int HISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];
int    SMOOTHHISTOGRAM[CF_OBSERVABLES][7][CF_GRAINS];
char   ERASE[CF_BUFSIZE];
double AGE;

static struct Averages MAX,MIN;

char OUTPUTDIR[CF_BUFSIZE],*sp;
char REMOVEHOSTS[CF_BUFSIZE] = {0};
char NOVA_EXPORT_TYPE[CF_MAXVARSIZE] = {0};
char NOVA_IMPORT_FILE[CF_MAXVARSIZE] = {0};

int HUBQUERY = false;
char PROMISEHANDLE[CF_MAXVARSIZE] = {0};
char HOSTKEY[CF_MAXVARSIZE] = {0};
char CLASSREGEX[CF_MAXVARSIZE] = {0};
char LSDATA[CF_MAXVARSIZE] = {0};
char NAME[CF_MAXVARSIZE] = {0};

FILE *FPAV=NULL,*FPVAR=NULL, *FPNOW=NULL;
FILE *FPE[CF_OBSERVABLES],*FPQ[CF_OBSERVABLES];
FILE *FPM[CF_OBSERVABLES];

struct Rlist *REPORTS = NULL;
struct Rlist *CSVLIST = NULL;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

const char *ID = "The reporting agent is a merger between the older\n"
                 "cfengine programs cfshow and cfenvgraph. It outputs\n"
                 "data stored in cfengine's embedded databases in human\n"
                 "readable form.";

const struct option OPTIONS[32] =
      {
      { "help",no_argument,0,'h' },
      { "class-regex",required_argument,0,'c'},
      { "csv",no_argument,0,'C'},
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "inform",no_argument,0,'I' },
      { "version",no_argument,0,'V' },
      { "no-lock",no_argument,0,'K'},
      { "file",required_argument,0,'f' },
      { "hostkey",required_argument,0,'k'},
      { "html",no_argument,0,'H'},
      { "xml",no_argument,0,'X'},
      { "version",no_argument,0,'V'},
      { "purge",no_argument,0,'P'},
      { "erasehistory",required_argument,0,'E' },
      { "nova-export",required_argument,0,'x'},
      { "nova-import",required_argument,0,'i'},
      { "outputdir",required_argument,0,'o' },
      { "promise-handle",required_argument,0,'p'},
      { "query-hub",optional_argument,0,'q'},
      { "titles",no_argument,0,'t'},
      { "timestamps",no_argument,0,'T'},
      { "resolution",no_argument,0,'R'},
      { "show",required_argument,0,'1'},      
      { "syntax",no_argument,0,'S'},
      { "syntax-export",no_argument,0,'s'},
      { "no-error-bars",no_argument,0,'e'},
      { "no-scaling",no_argument,0,'n'},
      { "verbose",no_argument,0,'v'},
      { "remove-hosts",required_argument,0,'r'},
      { NULL,0,0,'\0' }
      };

const char *HINTS[32] =
      {
      "Print the help message",
      "Specify a class regular expression to search for",
      "Enable CSV output mode in hub queries",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output information about actions performed by the agent",
      "Output the version of the software",
      "Ignore ifelapsed locks",
      "Specify an alternative input file than the default",
      "Specify a hostkey to lookup",
      "Print output in HTML",
      "Print output in XML",
      "Print version string for software",
      "Purge data about peers not seen beyond the threshold horizon for assumed-dead",
      "Erase historical data from the cf-monitord monitoring database",
      "Export Nova reports to file - delta or full report",
      "Import Nova reports from file - specify the path (only on Nova policy hub)",
      "Set output directory for printing graph data",
      "Specify a promise-handle to look up",
      "Query hub database interactively with optional regex search string",
      "Add title data to generated graph files",
      "Add a time stamp to directory name for graph file data",
      "Print graph data in high resolution",
      "Show data matching named criteria (software,variables,classes)",
      "Print a syntax summary for this cfengine version",
      "Export a syntax tree in Javascript format",
      "Do not add error bars to the printed graphs",
      "Do not automatically scale the axes",
      "Generate verbose output",
      "Remove comma separated list of key hash entries from the hosts-seen database",
      NULL
      };

#define CF_ACTIVE 1
#define CF_INACTIVE 0

/*******************************************************************/

struct CEnt /* For sorting */
   {
   char name[256];
   char date[32];
   double q;
   double d;
   };

/*******************************************************************/

enum cf_format
   {
   cfx_entry,
   cfx_event,
   cfx_host,
   cfx_pm,
   cfx_ip,
   cfx_date,
   cfx_q,
   cfx_av,
   cfx_dev,
   cfx_version,
   cfx_ref,
   cfx_filename,
   cfx_index,
   cfx_min,
   cfx_max,
   cfx_end,
   cfx_alias
   };

char *CFRX[][2] =
   {
    {"<entry>\n","\n</entry>\n"},
    {"<event>\n","\n</event>\n"},
    {"<hostname>\n","\n</hostname>\n"},
    {"<pm>\n","\n</pm>\n"},
    {"<ip>\n","\n</ip>\n"},
    {"<date>\n","\n</date>\n"},
    {"<q>\n","\n</q>\n"},
    {"<expect>\n","\n</expect>\n"},
    {"<sigma>\n","\n</sigma>\n"},
    {"<version>\n","\n</version>\n"},
    {"<ref>\n","\n</ref>\n"},
    {"<filename>\n","\n</filename>\n"},
    {"<index>\n","\n</index>\n"},
    {"<min>\n","\n</min>\n"},
    {"<max>\n","\n</max>\n"},
    {"<end>\n","\n</end>\n"},
    {"<alias>\n","\n</alias>\n"},
    {NULL,NULL}
   };

char *CFRH[][2] =
   {
    {"<tr>","</tr>\n\n"},
    {"<td>","</td>\n"},
    {"<td>","</td>\n"},
    {"<td bgcolor=#add8e6>","</td>\n"},
    {"<td bgcolor=#e0ffff>","</td>\n"},
    {"<td bgcolor=#f0f8ff>","</td>\n"},
    {"<td bgcolor=#fafafa>","</td>\n"},
    {"<td bgcolor=#ededed>","</td>\n"},
    {"<td bgcolor=#e0e0e0>","</td>\n"},
    {"<td bgcolor=#add8e6>","</td>\n"},
    {"<td bgcolor=#e0ffff>","</td>\n"},
    {"<td bgcolor=#fafafa><small>","</small></td>\n"},
    {"<td bgcolor=#fafafa><small>","</small></td>\n"},
    {"<td>","</td>\n"},
    {"<td>","</td>\n"},
    {"<td>","</td>\n"},
    {"<td>","</td>\n"},
    {NULL,NULL}
   };

/*****************************************************************************/

int main(int argc,char *argv[])

{ 
CheckOpts(argc,argv);
GenericInitialize(argc,argv,"reporter");
ThisAgentInit();
KeepReportsControlPromises();
KeepReportsPromises();
GenericDeInitialize();
return 0;
}



/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  int optindex = 0;
  int c;

while ((c=getopt_long(argc,argv,"Cghd:vVf:st:ar:PXHLMIRSKE:x:i:q:1:p:k:c:",OPTIONS,&optindex)) != EOF)
   {
   switch ((char) c)
      {
      case 'f':
          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          MINUSF = true;
          break;

      case 'd':
          switch ((optarg==NULL) ? '3' : *optarg)
             {
             case '1':
                 D1 = true;
                 DEBUG = true;
                 break;
             case '2':
                 D2 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;

      case 'S':
          SyntaxTree();
          exit(0);
          break;

      case 's':
          SyntaxExport();
          exit(0);
          break;

      case'K':
          IGNORELOCK = true;
          break;

      case 'v':
          VERBOSE = true;
          break;

      case 'I':
          INFORM = true;
          break;

      case 'V':
          PrintVersionBanner("cf-report");
          exit(0);

      case 'h':
          Syntax("cf-report - cfengine's reporting agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M':
          ManPage("cf-report - cfengine's reporting agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'E':
          strncpy(ERASE,optarg,CF_BUFSIZE-1);
          break;

      case 't':
          TITLES = true;
          break;

      case 'o': strcpy(OUTPUTDIR,optarg);
          CfOut(cf_inform,"","Setting output directory to s\n",OUTPUTDIR);
          break;

      case 'T': TIMESTAMPS = true;
          break;

      case 'R': HIRES = true;
         break;

      case 'r':
          if(snprintf(REMOVEHOSTS, sizeof(REMOVEHOSTS), "%s", optarg) >= sizeof(REMOVEHOSTS))
             {
             CfOut(cf_error, "", "List of hosts to remove is too long");
             exit(1);
             }
          break;         

      case 'e': ERRORBARS = false;
          break;

      case 'n': NOSCALING = true;
          break;

      case 'N': NOWOPT = true;
          break;

      case 'X':
          XML = true;
          break;

      case 'H':
          HTML = true;
          break;

      case 'C':
          CSV = true;
          break;
          
      case 'P':
          PURGE = 'y';
          break;

      case 'x':          
          if((String2Menu(optarg) != cfd_menu_delta) &&
             (String2Menu(optarg) != cfd_menu_full))
             {
             Syntax("Wrong argument to export: should be delta or full",OPTIONS,HINTS,ID);
             exit(1);
             }
          
          snprintf(NOVA_EXPORT_TYPE, sizeof(NOVA_EXPORT_TYPE), "%s", optarg);
          break;

      case 'i':          
          snprintf(NOVA_IMPORT_FILE, sizeof(NOVA_IMPORT_FILE), "%s", optarg);
          break;

      /* Some options for querying hub data - only on commercial hubs */

      case 'q':
          HUBQUERY = true;
          if (optarg)
             {
             strcpy(NAME,optarg);
             }
          break;
      case '1':
          strcpy(LSDATA,optarg);
          break;
      case 'p':
          strcpy(PROMISEHANDLE,optarg);
          break;
      case 'k':
          strcpy(HOSTKEY,optarg);
          break;
      case 'c':
          strcpy(CLASSREGEX,optarg);
          break;          

      default: Syntax("cf-report - cfengine's reporting agent",OPTIONS,HINTS,ID);
          exit(1);

      }
  }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }

}

/*****************************************************************************/

void ThisAgentInit()

{
time_t now;

if (strlen(OUTPUTDIR) == 0)
   {
   if (TIMESTAMPS)
      {
      if ((now = time((time_t *)NULL)) == -1)
         {
         CfOut(cf_verbose,"","Couldn't read system clock\n");
         }

      sprintf(OUTPUTDIR,"cf-reports-%s-%s",CanonifyName(VFQNAME),cf_ctime(&now));
      }
   else
      {
      sprintf(OUTPUTDIR,"cf-reports-%s",CanonifyName(VFQNAME));
      }
   }

XML = false;
HTML = false;

umask(077);
strcpy(ERASE,"");
strcpy(STYLESHEET,"");
strcpy(WEBDRIVER,"#");
strcpy(BANNER,"");
strcpy(FOOTER,"");
snprintf(VINPUTFILE,CF_MAXVARSIZE,"%s/state/%s",CFWORKDIR,CF_AVDB_FILE);
MapName(VINPUTFILE);

if (!EMPTY(REMOVEHOSTS))
   {
   RemoveHostSeen(REMOVEHOSTS);
   GenericDeInitialize();
   exit(0);
   }

#ifdef HAVE_NOVA
if (HUBQUERY)
   {
   int count = 0;

   if (strlen(PROMISEHANDLE) > 0)
      {
      count++;
      }

   if (strlen(HOSTKEY) > 0)
      {
      count++;
      }

   if (strlen(CLASSREGEX) > 0)
      {
      count++;
      }

   if (count > 1)
      {
      CfOut(cf_error,""," !! You can only specify one of the following at a time: --promise");
      FatalError("Aborted");
      }

   Nova_CommandAPI(LSDATA,NAME,PROMISEHANDLE,HOSTKEY,CLASSREGEX);
   exit(0);
   }

#endif

#ifdef HAVE_NOVA
if (!EMPTY(NOVA_EXPORT_TYPE))
   {
   if(Nova_ExportReports(NOVA_EXPORT_TYPE))
      {
      GenericDeInitialize();
      exit(0);
      }
   else
      {
      GenericDeInitialize();
      exit(1);
      }
   }

if (!EMPTY(NOVA_IMPORT_FILE))
   {
   if (IsDefinedClass("am_policy_hub"))
      {
      if(Nova_ImportHostReports(NOVA_IMPORT_FILE))
         {
         GenericDeInitialize();
         exit(0);
         }
      else
         {
         GenericDeInitialize();
         exit(1);
         }
      }
   else
      {
      CfOut(cf_error, "", "Importing reports is only possible on Nova policy hubs");
      }
   }
#endif
}

/*****************************************************************************/

void KeepReportsControlPromises()

{ struct Constraint *cp;
  struct Rlist *rp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_report); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_reporter",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in report agent control body",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_builddir].lval) == 0)
      {
      strncpy(OUTPUTDIR,retval,CF_BUFSIZE);
      CfOut(cf_verbose,"","SET outputdir = %s\n",OUTPUTDIR);

      if (cf_mkdir(OUTPUTDIR,0755) == -1)
         {
         CfOut(cf_verbose,"","Writing to existing directory\n");
         }

      if (chdir(OUTPUTDIR))
         {
         CfOut(cf_error,"chdir","Could not set the working directory to %s",OUTPUTDIR);
         exit(0);
         }

      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_autoscale].lval) == 0)
      {
      NOSCALING = !GetBoolean(retval);
      CfOut(cf_verbose,"","SET autoscale = %d\n",NOSCALING);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_html_embed].lval) == 0)
      {
      EMBEDDED = GetBoolean(retval);
      CfOut(cf_verbose,"","SET html_embedding = %d\n",EMBEDDED);
      continue;
      }


   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_timestamps].lval) == 0)
      {
      TIMESTAMPS = GetBoolean(retval);
      CfOut(cf_verbose,"","SET timestamps = %d\n",TIMESTAMPS);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_errorbars].lval) == 0)
      {
      ERRORBARS = GetBoolean(retval);
      CfOut(cf_verbose,"","SET errorbars = %d\n",ERRORBARS);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_query_engine].lval) == 0)
      {
      strncpy(WEBDRIVER,retval,CF_MAXVARSIZE);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_aggregation_point].lval) == 0)
      {
      /* Ignore for backward compatibility */
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_htmlbanner].lval) == 0)
      {
      strncpy(BANNER,retval,CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_htmlfooter].lval) == 0)
      {
      strncpy(FOOTER,retval,CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_report_output].lval) == 0)
      {
      if (strcmp("html",retval) == 0)
         {
         HTML = true;
         }
      else if (strcmp("xml",retval) == 0)
         {
         XML = true;
         }
      else if (strcmp("csv",retval) == 0)
         {
         CSV = true;
         }
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_stylesheet].lval) == 0)
      {
      strncpy(STYLESHEET,retval,CF_MAXVARSIZE);
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_reports].lval) == 0)
      {
      for (rp  = (struct Rlist *)retval; rp != NULL; rp = rp->next)
         {
         IdempPrependRScalar(&REPORTS,rp->item,CF_SCALAR);
         CfOut(cf_inform,"","Adding %s to the reports...\n",rp->item);
         }
      continue;
      }

   if (strcmp(cp->lval,CFRE_CONTROLBODY[cfre_csv].lval) == 0)
      {
      for (rp  = (struct Rlist *)retval; rp != NULL; rp = rp->next)
         {
         IdempPrependRScalar(&CSVLIST,rp->item,CF_SCALAR);
         CfOut(cf_inform,"","Adding %s to the csv2xml list...\n",rp->item);
         }
      continue;
      }
   }

if (GetVariable("control_common",CFG_CONTROLBODY[cfg_lastseenexpireafter].lval,&retval,&rettype) != cf_notype)
   {
   LASTSEENEXPIREAFTER = Str2Int(retval);
   }
}

/*****************************************************************************/

void KeepReportsPromises()

{ struct Rlist *rp;
  int all = false;

if (REPORTS == NULL)
   {
   CfOut(cf_error,""," !! Nothing to report - nothing selected\n");
   exit(0);
   }

CfOut(cf_verbose,""," -> Creating sub-directory %s\n",OUTPUTDIR);

if (cf_mkdir(OUTPUTDIR,0755) == -1)
   {
   CfOut(cf_verbose,""," -> Writing to existing directory\n");
   }

if (chdir(OUTPUTDIR))
   {
   CfOut(cf_error,"chdir"," !! Could not set the working directory");
   exit(0);
   }

ReadAverages();

for (rp  = REPORTS; rp != NULL; rp = rp->next)
   {
   Banner(rp->item);

   if (strcmp("all",rp->item) == 0)
      {
      if (RlistLen(REPORTS) > 1)
         {
         CfOut(cf_error,""," !! \"all\" should be the only item in the list, if it exists");
         continue;
         }
      
      all = true;            
      }
   
   if (all || strcmp("last_seen",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating last-seen report...\n");
      ShowLastSeen();
      }

   if (strcmp("all_locks",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating lock report...\n");
      ShowLocks(CF_INACTIVE);
      }

   if (strcmp("active_locks",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating active lock report...\n");
      ShowLocks(CF_ACTIVE);
      }

   if (strcmp("hashes",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating file-hash report...\n");
      ShowChecksums();
      }

   if (all || strcmp("performance",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating performance report...\n");
      ShowPerformance();
      }

   if (strcmp("audit",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating audit report...\n");
      ShowCurrentAudit();
      }

   if (all || strcmp("classes",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating classes report...\n");
      ShowClasses();
      }

   if (all || strcmp("monitor_now",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating monitor recent-history report...\n");
      MagnifyNow();
      }

   if (all || strcmp("monitor_summary",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating monitor history summary...\n");
      SummarizeAverages();
      }

   if (all || strcmp("monitor_history",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating monitor full history report...\n");
      WriteGraphFiles();
      WriteHistograms();
      DiskArrivals();
      PeerIntermittency();
      LongHaul();
      }

   if (all || strcmp("compliance",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating compliance summary (Cfengine Nova and above)...\n");
      SummarizeCompliance(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      CfOut(cf_verbose,""," -> Creating per-promise compliance summary (Cfengine Nova and above)...\n");
      SummarizePerPromiseCompliance(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      CfOut(cf_verbose,""," -> Creating promise repair summary (Cfengine Nova and above)...\n");
      SummarizePromiseRepaired(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      CfOut(cf_verbose,""," -> Creating promise non-kept summary (Cfengine Nova and above)...\n");
      SummarizePromiseNotKept(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("file_changes",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating file change summary (Cfengine Nova and above)...\n");
      SummarizeFileChanges(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("installed_software",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating software version summary (Cfengine Nova and above)...\n");
      SummarizeSoftware(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("software_patches",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating software update version summary (Cfengine Nova and above)...\n");
      SummarizeUpdates(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("setuid",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating setuid report (Cfengine Nova and above)...\n");
      SummarizeSetuid(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("variables",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating variables report (Cfengine Nova and above)...\n");
      SummarizeVariables(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }

   if (all || strcmp("value",rp->item) == 0)
      {
      CfOut(cf_verbose,""," -> Creating value report (Cfengine Nova and above)...\n");
      SummarizeValue(XML,HTML,CSV,EMBEDDED,STYLESHEET,BANNER,FOOTER,WEBDRIVER);
      }
   }

if (CSVLIST)
   {
   CSV2XML(CSVLIST);
   }

if (strlen(ERASE) > 0)
   {
   EraseAverages();
   exit(0);
   }

/* Compute summaries */

GrandSummary();
}

/*********************************************************************/
/* Level 2                                                           */
/*********************************************************************/

void RemoveHostSeen(char *hosts)

{ CF_DB *dbp;
  char name[CF_MAXVARSIZE];
  struct Item *ip,*list = SplitStringAsItemList(hosts,',');

snprintf(name,sizeof(name),"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   CfOut(cf_error, "", "!! Could not open hosts-seen database for removal of hosts");
   return;
   }

for (ip = list; ip != NULL; ip=ip->next)
   {
   snprintf(name,sizeof(name),"+%s",ip->name);
   CfOut(cf_inform,""," -> Deleting requested host-seen entry for %s\n",name);
   if(!DeleteDB(dbp,name))
      {
      CfOut(cf_inform, "", " !! Entry %s not found - skipping", name);
      }

   snprintf(name,sizeof(name),"-%s",ip->name);
   CfOut(cf_inform,""," -> Deleting requested host-seen entry for %s\n",name);
   if(!DeleteDB(dbp,name))
      {
      CfOut(cf_inform, "", " !! Entry %s not found - skipping", name);
      }
   }

CloseDB(dbp);
DeleteItemList(list);
}

/*********************************************************************/

void ShowLastSeen()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  FILE *fout;
  time_t tid = time(NULL);
  double now = (double)tid,average = 0, var = 0;
  double ticksperhr = (double)SECONDS_PER_HOUR;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],address[CF_MAXVARSIZE];
  struct CfKeyHostSeen entry;
  int ksize,vsize;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"lastseen.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"lastseen.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"lastseen.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"lastseen.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/lastseen.html\n",OUTPUTDIR);
   exit(1);
   }

if (HTML && !EMBEDDED)
   {
   snprintf(name,CF_BUFSIZE,"Peers as last seen by %s",VFQNAME);
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<div id=\"reporttext\">\n");
   fprintf(fout,"<h4>This report was last updated at %s</h4>",cf_ctime(&tid));
   fprintf(fout,"<table class=border cellpadding=5>\n");
   }
else if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen database");
   CloseDB(dbp);
   return;
   }

 /* Initialize the key/data return pair. */

memset(&entry, 0, sizeof(entry));

 /* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   double then;
   time_t fthen;
   char tbuf[CF_BUFSIZE],addr[CF_BUFSIZE];

   memcpy(&then,value,sizeof(then));
   
   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));
      strncpy(hostname,(char *)key,ksize);
      strncpy(address,(char *)entry.address,ksize);   
      then = entry.Q.q;
      average = (double)entry.Q.expect;
      var = (double)entry.Q.var;
      strncpy(addr,entry.address,CF_MAXVARSIZE);
      }
   else
      {
      continue;
      }

   if (now - then > (double)LASTSEENEXPIREAFTER)
      {
      DeleteDB(dbp,key);
      CfOut(cf_inform,""," -> Deleting expired entry for %s\n",hostname);
      continue;
      }

   fthen = (time_t)then;                            /* format date */
   snprintf(tbuf,CF_BUFSIZE-1,"%s",cf_ctime(&fthen));
   tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

   CfOut(cf_verbose,""," -> Reporting on %s",hostname);
   
   if (XML)
      {
      fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
      fprintf(fout,"%s%c%s",CFRX[cfx_pm][cfb],*hostname,CFRX[cfx_pm][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_host][cfb],hostname+1,CFRX[cfx_host][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_alias][cfb],IPString2Hostname(address),CFRX[cfx_ip][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_ip][cfb],address,CFRX[cfx_ip][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],tbuf,CFRX[cfx_date][cfe]);
      fprintf(fout,"%s%.2lf%s",CFRX[cfx_q][cfb],((double)(now-then))/ticksperhr,CFRX[cfx_q][cfe]);
      fprintf(fout,"%s%.2lf%s",CFRX[cfx_av][cfb],average/ticksperhr,CFRX[cfx_av][cfe]);
      fprintf(fout,"%s%.2lf%s",CFRX[cfx_dev][cfb],sqrt(var)/ticksperhr,CFRX[cfx_dev][cfe]);
      fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
      }
   else if (HTML)
      {
      fprintf(fout,"%s",CFRH[cfx_entry][cfb]);

      if (*hostname == '+')
         {
         fprintf(fout,"%s out (%c)%s",CFRH[cfx_pm][cfb],*hostname,CFRH[cfx_pm][cfe]);
         }
      else
         {
         fprintf(fout,"%s in (%c)%s",CFRH[cfx_pm][cfb],*hostname,CFRH[cfx_pm][cfe]);
         }
      fprintf(fout,"%s%s%s",CFRH[cfx_host][cfb],hostname+1,CFRH[cfx_host][cfe]);
      fprintf(fout,"%s%s%s",CFRH[cfx_ip][cfb],address,CFRH[cfx_ip][cfe]);
      fprintf(fout,"%s%s%s",CFRH[cfx_alias][cfb],IPString2Hostname(address),CFRH[cfx_ip][cfe]);
      fprintf(fout,"%s Last seen at %s%s",CFRH[cfx_date][cfb],tbuf,CFRH[cfx_date][cfe]);
      fprintf(fout,"%s %.2lf hrs ago %s",CFRH[cfx_q][cfb],((double)(now-then))/ticksperhr,CFRH[cfx_q][cfe]);
      fprintf(fout,"%s Av %.2lf hrs %s",CFRH[cfx_av][cfb],average/ticksperhr,CFRH[cfx_av][cfe]);
      fprintf(fout,"%s &plusmn; %.2lf hrs %s",CFRH[cfx_dev][cfb],sqrt(var)/ticksperhr,CFRH[cfx_dev][cfe]);
      fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
      }
   else if (CSV)
      {
      fprintf(fout,"%c,%25.25s,%25.25s,%15.15s,%s,%.2lf,%.2lf,%.2lf hrs\n",
             *hostname,
             hostname+1,
             IPString2Hostname(address),
             addr,
             tbuf,
             ((double)(now-then))/ticksperhr,
             average/ticksperhr,
             sqrt(var)/ticksperhr);
      }
   else
      {
      fprintf(fout,"IP %c %25.25s %25.25s %15.15s  @ [%s] not seen for (%.2lf) hrs, Av %.2lf +/- %.2lf hrs\n",
             *hostname,
             hostname+1,
             IPString2Hostname(address),
             addr,
             tbuf,
             ((double)(now-then))/ticksperhr,
             average/ticksperhr,
             sqrt(var)/ticksperhr);
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table></div>\n");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
}

/*******************************************************************/

void ShowPerformance()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  FILE *fout;
  double now = (double)time(NULL),average = 0, var = 0;
  double ticksperminute = 60.0;
  char name[CF_BUFSIZE],eventname[CF_BUFSIZE];
  struct Event entry;
  int ksize,vsize;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_PERFORMANCE);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan hash database");
   return;
   }

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"performance.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"performance.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"performance.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"performance.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/%s\n",OUTPUTDIR,name);
   exit(1);
   }

 /* Initialize the key/data return pair. */

memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
memset(&entry, 0, sizeof(entry));

if (HTML && !EMBEDDED)
   {
   snprintf(name,CF_BUFSIZE,"Promises last kept by %s",VFQNAME);
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<div id=\"reporttext\">\n");
   fprintf(fout,"<table class=border cellpadding=5>\n");
   }

if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   double measure;
   time_t then;
   char tbuf[CF_BUFSIZE];

   memcpy(&then,value,sizeof(then));
   strncpy(eventname,(char *)key,ksize);

   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));

      then    = entry.t;
      measure = entry.Q.q/ticksperminute;
      average = entry.Q.expect/ticksperminute;
      var     = entry.Q.var;

      snprintf(tbuf,CF_BUFSIZE-1,"%s",cf_ctime(&then));
      tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

      if (PURGE == 'y')
         {
         if (now - then > CF_WEEK)
            {
            DeleteDB(dbp,key);
            }

         CfOut(cf_inform,"","Deleting expired entry for %s\n",eventname);

         if (measure < 0 || average < 0 || measure > 4*CF_WEEK)
            {
            DeleteDB(dbp,key);
            }

         CfOut(cf_inform,""," -> Deleting entry for %s because it seems to take longer than 4 weeks to complete\n",eventname);

         continue;
         }

      if (XML)
         {
         fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
         fprintf(fout,"%s%s%s",CFRX[cfx_event][cfb],eventname,CFRX[cfx_event][cfe]);
         fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],tbuf,CFRX[cfx_date][cfe]);
         fprintf(fout,"%s%.4lf%s",CFRX[cfx_q][cfb],measure,CFRX[cfx_q][cfe]);
         fprintf(fout,"%s%.4lf%s",CFRX[cfx_av][cfb],average,CFRX[cfx_av][cfe]);
         fprintf(fout,"%s%.4lf%s",CFRX[cfx_dev][cfb],sqrt(var)/ticksperminute,CFRX[cfx_dev][cfe]);
         fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
         }
      else if (HTML)
         {
         fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
         fprintf(fout,"%s%s%s",CFRH[cfx_event][cfb],eventname,CFRH[cfx_event][cfe]);
         fprintf(fout,"%s last performed at %s%s",CFRH[cfx_date][cfb],tbuf,CFRH[cfx_date][cfe]);
         fprintf(fout,"%s completed in %.4lf mins %s",CFRH[cfx_q][cfb],measure,CFRH[cfx_q][cfe]);
         fprintf(fout,"%s Av %.4lf mins %s",CFRH[cfx_av][cfb],average,CFRH[cfx_av][cfe]);
         fprintf(fout,"%s &plusmn; %.4lf mins %s",CFRH[cfx_dev][cfb],sqrt(var)/ticksperminute,CFRH[cfx_dev][cfe]);
         fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
         }
      else if (CSV)
         {
         fprintf(fout,"%7.4lf,%s,%7.4lf,%7.4lf,%s \n",measure,tbuf,average,sqrt(var)/ticksperminute,eventname);
         }
      else
         {
         fprintf(fout,"(%7.4lf mins @ %s) Av %7.4lf +/- %7.4lf for %s \n",measure,tbuf,average,sqrt(var)/ticksperminute,eventname);
         }
      }
   else
      {
      continue;
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   fprintf(fout,"</div>\n");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
}


/*******************************************************************/

void ShowClasses()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  FILE *fout,*fnotes;
  struct Item *already = NULL,*ip;
  double now = (double)time(NULL),average = 0, var = 0;
  char name[CF_BUFSIZE],eventname[CF_BUFSIZE];
  struct Event entry;
  struct CEnt array[1024];
  int i,ksize,vsize;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_CLASSUSAGE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan class db");
   return;
   }

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"classes.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"classes.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"classes.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"classes.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/%s\n",OUTPUTDIR,name);
   exit(1);
   }

/* Initialize the key/data return pair. */

memset(&entry, 0, sizeof(entry));

if (HTML && !EMBEDDED)
   {
   time_t now = time(NULL);
   snprintf(name,CF_BUFSIZE,"Classes last observed on %s at %s",VFQNAME,cf_ctime(&now));
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<div id=\"reporttext\">\n");

   fprintf(fout,"<h4>Soft classes</h4>");
   fprintf(fout,"<table class=\"border\" cellpadding=\"5\">\n");
   }

if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

for (i = 0; i < 1024; i++)
   {
   *(array[i].name) = '\0';
   array[i].q = -1;
   }

i = 0;

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   double measure;
   time_t then;
   char tbuf[CF_BUFSIZE];

   memcpy(&then,value,sizeof(then));
   strncpy(eventname,(char *)key,CF_BUFSIZE-1);

   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));

      then    = entry.t;
      measure = entry.Q.q;
      average = entry.Q.expect;
      var     = entry.Q.var;

      snprintf(tbuf,CF_BUFSIZE-1,"%s",cf_ctime(&then));
      tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

      if (PURGE == 'y')
         {
         if (now - then > CF_WEEK*52)
            {
            DeleteDB(dbp,key);
            }

         CfOut(cf_error,""," -> Deleting expired entry for %s\n",eventname);
         continue;
         }

      if (i++ < 1024)
         {
         strncpy(array[i].date,tbuf,31);
         strncpy(array[i].name,eventname,254);
         array[i].q = average;
         array[i].d = var;
         }
      else
         {
         break;
         }
      }
   }

#ifdef HAVE_QSORT
qsort(array,1024,sizeof(struct CEnt),CompareClasses);
#endif

if ((fnotes = fopen("class_notes","w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/class_notes\n",OUTPUTDIR);
   exit(1);
   }

for (i = 0; i < 1024; i++)
   {
   if (array[i].q <= 0.00001)
      {
      continue;
      }

   fprintf(fnotes,"%s %.4lf\n",array[i].name,array[i].q);

   PrependItem(&already,array[i].name,NULL);
   
   if (XML)
      {
      fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
      fprintf(fout,"%s%s%s",CFRX[cfx_event][cfb],array[i].name,CFRX[cfx_event][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],array[i].date,CFRX[cfx_date][cfe]);
      fprintf(fout,"%s%.4lf%s",CFRX[cfx_av][cfb],array[i].q,CFRX[cfx_av][cfe]);
      fprintf(fout,"%s%.4lf%s",CFRX[cfx_dev][cfb],sqrt(array[i].d),CFRX[cfx_dev][cfe]);
      fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
      }
   else if (HTML)
      {
      fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
      fprintf(fout,"%s%s%s",CFRH[cfx_event][cfb],array[i].name,CFRH[cfx_event][cfe]);
      fprintf(fout,"%s last occured at %s%s",CFRH[cfx_date][cfb],array[i].date,CFRH[cfx_date][cfe]);
      fprintf(fout,"%s Probability %.4lf %s",CFRH[cfx_av][cfb],array[i].q,CFRH[cfx_av][cfe]);
      fprintf(fout,"%s &plusmn; %.4lf %s",CFRH[cfx_dev][cfb],sqrt(array[i].d),CFRH[cfx_dev][cfe]);
      fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
      }
   else if (CSV)
      {
      fprintf(fout,"%7.4lf,%7.4lf,%s,%s\n",array[i].q,sqrt(array[i].d),array[i].name,array[i].date);
      }
   else
      {
      fprintf(fout,"Probability %7.4lf +/- %7.4lf for %s (last observed @ %s)\n",array[i].q,sqrt(array[i].d),array[i].name,array[i].date);
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   fprintf(fout,"<h4>All classes</h4>\n");
   fprintf(fout,"<table class=\"border\" cellpadding=\"5\">\n");
   }

for (i = 0; i < CF_ALPHABETSIZE; i++)
   {
   for (ip = VHEAP.list[i]; ip != NULL; ip=ip->next)
      {
      if (IsItemIn(already,ip->name))
         {
         continue;
         }
      
      if (strncmp(ip->name,"Min",3) == 0 || strncmp(ip->name,"Hr",2) == 0 || strncmp(ip->name,"Q",1) == 0
          || strncmp(ip->name,"Yr",1) == 0 || strncmp(ip->name,"Day",1) == 0 || strncmp(ip->name,"Morning",1) == 0
          || strncmp(ip->name,"Afternoon",1) == 0 || strncmp(ip->name,"Evening",1) == 0 || strncmp(ip->name,"Night",1) == 0)
         {
         continue;
         }
      
      if (XML)
         {
         fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
         fprintf(fout,"%s%s%s",CFRX[cfx_event][cfb],ip->name,CFRX[cfx_event][cfe]);
         fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],"often",CFRX[cfx_date][cfe]);
         fprintf(fout,"%s%.4lf%s",CFRX[cfx_av][cfb],1.0,CFRX[cfx_av][cfe]);
         fprintf(fout,"%s%.4lf%s",CFRX[cfx_dev][cfb],0.0,CFRX[cfx_dev][cfe]);
         fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
         }
      else if (HTML)
         {
         fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
         fprintf(fout,"%s%s%s",CFRH[cfx_event][cfb],ip->name,CFRH[cfx_event][cfe]);
         fprintf(fout,"%s occurred %s%s",CFRH[cfx_date][cfb],"often",CFRH[cfx_date][cfe]);
         fprintf(fout,"%s Probability %.4lf %s",CFRH[cfx_av][cfb],1.0,CFRH[cfx_av][cfe]);
         fprintf(fout,"%s &plusmn; %.4lf %s",CFRH[cfx_dev][cfb],0.0,CFRH[cfx_dev][cfe]);
         fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
         }
      else if (CSV)
         {
         fprintf(fout,"%7.4lf,%7.4lf,%s,%s\n",1.0,0.0,ip->name,"often");
         }
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   fprintf(fout,"</div>\n");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
fclose(fnotes);
DeleteItemList(already);
}

/*******************************************************************/

void ShowChecksums()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  int ksize,vsize;
  FILE *fout;
  char checksumdb[CF_BUFSIZE],name[CF_BUFSIZE];

snprintf(checksumdb,CF_BUFSIZE,"%s/%s",CFWORKDIR,CF_CHKDB);

if (!OpenDB(checksumdb,&dbp))
   {
   return;
   }

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"hashes.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"hashes.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"hashes.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"hashes.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/%s\n",OUTPUTDIR,name);
   exit(1);
   }

if (HTML && !EMBEDDED)
   {
   CfHtmlHeader(fout,"File hashes",STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<div id=\"reporttext\">\n");
   fprintf(fout,"<table class=border cellpadding=5>\n");
   }

if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan checksum db");
   CloseDB(dbp);
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   enum cfhashes type;
   char strtype[CF_MAXVARSIZE];
   char name[CF_BUFSIZE];
   struct Checksum_Value chk_val;
   unsigned char digest[EVP_MAX_MD_SIZE+1];

   memcpy(&chk_val,value,sizeof(chk_val));
   memcpy(digest,chk_val.mess_digest,EVP_MAX_MD_SIZE+1);

   strncpy(strtype,key,CF_INDEX_FIELD_LEN);
   strncpy(name,(char *)key+CF_INDEX_OFFSET,CF_BUFSIZE-1);

   type = String2HashType(strtype);

   if (XML)
      {
      fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
      fprintf(fout,"%s%s%s",CFRX[cfx_event][cfb],name,CFRX[cfx_event][cfe]);
      fprintf(fout,"%s%s%s",CFRX[cfx_q][cfb],HashPrint(type,digest),CFRX[cfx_q][cfe]);
      fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
      }
   else if (HTML)
      {
      fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
      fprintf(fout,"%s%s%s",CFRH[cfx_filename][cfb],name,CFRH[cfx_filename][cfe]);
      fprintf(fout,"%s%s%s",CFRH[cfx_q][cfb],HashPrint(type,digest),CFRH[cfx_q][cfe]);
      fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
      }
   else if (CSV)
      {
      fprintf(fout,"%s,",name);
      fprintf(fout,"%s\n",HashPrint(type,digest));
      }
   else
      {
      fprintf(fout,"%s = ",name);
      fprintf(fout,"%s\n",HashPrint(type,digest));
      }

   memset(&key,0,sizeof(key));
   memset(&value,0,sizeof(value));
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   fprintf(fout,"</div>\n");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
}

/*********************************************************************/

void ShowLocks (int active)

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  FILE *fout;
  int ksize,vsize;
  char lockdb[CF_BUFSIZE],name[CF_BUFSIZE];
  struct LockData entry;

snprintf(lockdb,CF_BUFSIZE,"%s/state/%s",CFWORKDIR, CF_LOCKDB_FILE);
MapName(lockdb);

if (!OpenDB(lockdb,&dbp))
   {
   return;
   }

if (active)
   {
   snprintf(lockdb,CF_MAXVARSIZE,"active");
   }
else
   {
   snprintf(lockdb,CF_MAXVARSIZE,"all");
   }

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"%s_locks.html",lockdb);
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"%s_locks.xml",lockdb);
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"%s_locks.csv",lockdb);
   }
else
   {
   snprintf(name,CF_BUFSIZE,"%s_locks.txt",lockdb);
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! Unable to write to %s/%s\n",OUTPUTDIR,name);
   CloseDB(dbp);
   return;
   }

if (HTML && !EMBEDDED)
   {
   time_t now = time(NULL);
   snprintf(name,CF_BUFSIZE,"%s lock data observed on %s at %s",lockdb,VFQNAME,cf_ctime(&now));
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<div id=\"reporttext\">\n");
   fprintf(fout,"<table class=border cellpadding=5>\n");
   }

if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen db");
   CloseDB(dbp);
   fclose(fout);
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   if (active)
      {
      if (value != NULL)
         {
         memcpy(&entry,value,sizeof(entry));
         }

      if (strncmp("lock",(char *)key,4) == 0)
         {
         if (XML)
            {
            fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
            fprintf(fout,"%s%s%s",CFRX[cfx_filename][cfb],(char *)key,CFRX[cfx_filename][cfe]);
            fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],cf_ctime(&entry.time),CFRX[cfx_date][cfe]);
            fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
            }
         else if (HTML)
            {
            fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
            fprintf(fout,"%s%s%s",CFRH[cfx_filename][cfb],(char *)key,CFRH[cfx_filename][cfe]);
            fprintf(fout,"%s%s%s",CFRH[cfx_date][cfb],cf_ctime(&entry.time),CFRH[cfx_date][cfe]);
            fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
            }
         else
            {
            fprintf(fout,"%s = ",(char *)key);
            fprintf(fout,"%s\n",cf_ctime(&entry.time));
            }

         CfOut(cf_inform,"","Active lock %s = ",(char *)key);
         }
      }
   else
      {
      if (strncmp("last",(char *)key,4) == 0)
         {
         if (XML)
            {
            fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
            fprintf(fout,"%s%s%s",CFRX[cfx_filename][cfb],(char *)key,CFRX[cfx_filename][cfe]);
            fprintf(fout,"%s%s%s",CFRX[cfx_date][cfb],cf_ctime(&entry.time),CFRX[cfx_date][cfe]);
            fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
            }
         else if (HTML)
            {
            fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
            fprintf(fout,"%s%s%s",CFRH[cfx_filename][cfb],(char *)key,CFRH[cfx_filename][cfe]);
            fprintf(fout,"%s%s%s",CFRH[cfx_date][cfb],cf_ctime(&entry.time),CFRH[cfx_date][cfe]);
            fprintf(fout,"%s",CFRH[cfx_entry][cfe]);
            }
         else if (CSV)
            {
            fprintf(fout,"%s",(char *)key);
            fprintf(fout,",%s\n",cf_ctime(&entry.time));
            }
         else
            {
            fprintf(fout,"%s = ",(char *)key);
            fprintf(fout,"%s\n",cf_ctime(&entry.time));
            }
         }
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   fprintf(fout,"</div>\n");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }


DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
}

/*******************************************************************/

void ShowCurrentAudit()

{ char operation[CF_BUFSIZE],name[CF_BUFSIZE];
  struct AuditLog entry;
  FILE *fout;
  char *key;
  void *value;
  CF_DB *dbp;
  CF_DBC *dbcp;
  int ksize,vsize;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_AUDITDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen db");
   return;
   }

memset(&entry, 0, sizeof(entry));

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"audit.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"audit.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"audit.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"audit.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Unable to write to %s/%s\n",OUTPUTDIR,name);
   return;
   }

if (HTML && !EMBEDDED)
   {
   time_t now = time(NULL);
   snprintf(name,CF_BUFSIZE,"Audit collected on %s at %s",VFQNAME,cf_ctime(&now));
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);

   fprintf(fout,"<table class=border cellpadding=5>\n");
   /* fprintf(fout,"<th> t-index </th>");*/
   fprintf(fout,"<th> Scan convergence </th>");
   fprintf(fout,"<th> Observed </th>");
   fprintf(fout,"<th> Promise made </th>");
   fprintf(fout,"<th> Promise originates in </th>");
   fprintf(fout,"<th> Promise version </th>");
   fprintf(fout,"<th> line </th>");
   }

if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

 /* Walk through the database and print out the key/data pairs. */

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   strncpy(operation,(char *)key,CF_BUFSIZE-1);

   if (value != NULL)
      {
      memcpy(&entry,value,sizeof(entry));

      if (XML)
         {
         fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
         fprintf(fout,"%s %s %s",CFRX[cfx_index][cfb],operation,CFRX[cfx_index][cfe]);
         fprintf(fout,"%s %s, ",CFRX[cfx_event][cfb],entry.operator);
         AuditStatusMessage(fout,entry.status);
         fprintf(fout,"%s",CFRX[cfx_event][cfe]);
         fprintf(fout,"%s %s %s",CFRX[cfx_q][cfb],entry.comment,CFRX[cfx_q][cfe]);
         fprintf(fout,"%s %s %s",CFRX[cfx_date][cfb],entry.date,CFRX[cfx_date][cfe]);
         fprintf(fout,"%s %s %s",CFRX[cfx_av][cfb],entry.filename,CFRX[cfx_av][cfe]);
         fprintf(fout,"%s %s %s",CFRX[cfx_version][cfb],entry.version,CFRX[cfx_version][cfe]);
         fprintf(fout,"%s %d %s",CFRX[cfx_ref][cfb],entry.lineno,CFRX[cfx_ref][cfe]);
         fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
         }
      else if (HTML)
         {
         fprintf(fout,"%s",CFRH[cfx_entry][cfb]);
         /* fprintf(fout,"%s %s %s",CFRH[cfx_index][cfb],operation,CFRH[cfx_index][cfe]);*/
         fprintf(fout,"%s %s, ",CFRH[cfx_event][cfb],Format(entry.operator,40));
         AuditStatusMessage(fout,entry.status);
         fprintf(fout,"%s",CFRH[cfx_event][cfe]);
         fprintf(fout,"%s %s %s",CFRH[cfx_q][cfb],Format(entry.comment,40),CFRH[cfx_q][cfe]);
         fprintf(fout,"%s %s %s",CFRH[cfx_date][cfb],entry.date,CFRH[cfx_date][cfe]);
         fprintf(fout,"%s %s %s",CFRH[cfx_av][cfb],entry.filename,CFRH[cfx_av][cfe]);
         fprintf(fout,"%s %s %s",CFRH[cfx_version][cfb],entry.version,CFRH[cfx_version][cfe]);
         fprintf(fout,"%s %d %s",CFRH[cfx_ref][cfb],entry.lineno,CFRH[cfx_ref][cfe]);
         fprintf(fout,"%s",CFRH[cfx_entry][cfe]);

         if (strstr(entry.comment,"closing"))
            {
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            fprintf(fout,"<th></th>");
            }
         }
      else if (CSV)
         {
         fprintf(fout,"%s,",operation);
         fprintf(fout,"%s,",entry.operator);

         AuditStatusMessage(fout,entry.status); /* Reminder */

         if (strlen(entry.comment) > 0)
            {
            fprintf(fout,",%s\n",entry.comment);
            }

         if (strcmp(entry.filename,"Terminal") == 0)
            {
            }
         else
            {
            if (strlen(entry.version) == 0)
               {
               fprintf(fout,",%s,,%s,%d\n",entry.filename,entry.date,entry.lineno);
               }
            else
               {
               fprintf(fout,",%s,%s,%s,%d\n",entry.filename,entry.version,entry.date,entry.lineno);
               }
            }
         }
      else
         {
         fprintf(fout,". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n");
         fprintf(fout,"Operation index: \'%s\'\n",operation);
         fprintf(fout,"Converge \'%s\' ",entry.operator);

         AuditStatusMessage(fout,entry.status); /* Reminder */

         if (strlen(entry.comment) > 0)
            {
            fprintf(fout,"Comment: %s\n",entry.comment);
            }

         if (strcmp(entry.filename,"Terminal") == 0)
            {
            if (strstr(entry.comment,"closing"))
               {
               fprintf(fout,"\n===============================================================================================\n\n");
               }
            }
         else
            {
            if (strlen(entry.version) == 0)
               {
               fprintf(fout,"Promised in %s (unamed version last edited at %s) at/before line %d\n",entry.filename,entry.date,entry.lineno);
               }
            else
               {
               fprintf(fout,"Promised in %s (version %s last edited at %s) at/before line %d\n",entry.filename,entry.version,entry.date,entry.lineno);
               }
            }
         }
      }
   else
      {
      continue;
      }
   }

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }


DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
fclose(fout);
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

char *Format(char *s,int width)

{ static char buffer[CF_BUFSIZE];
  char *sp;
  int i = 0, count = 0;

for (sp = s; *sp != '\0'; sp++)
   {
   buffer[i++] = *sp;
   buffer[i] = '\0';
   count++;

   if ((count > width - 5) && ispunct(*sp))
      {
      strcat(buffer,"<br>");
      i += strlen("<br>");
      count = 0;
      }
   }

return buffer;
}

/*************************************************************/

int CompareClasses(const void *a, const void *b)

{
struct CEnt *da = (struct CEnt *) a;
struct CEnt *db = (struct CEnt *) b;

return (da->q < db->q) - (da->q > db->q);
}

/****************************************************************************/

void ReadAverages()

{ struct Averages entry;
  char timekey[CF_MAXVARSIZE];
  time_t now;
  CF_DB *dbp;
  int i;

CfOut(cf_verbose,"","\nLooking for database %s\n",VINPUTFILE);
CfOut(cf_verbose,"","\nFinding MAXimum values...\n\n");
CfOut(cf_verbose,"","N.B. socket values are numbers in CLOSE_WAIT. See documentation.\n");

if (!OpenDB(VINPUTFILE,&dbp))
   {
   CfOut(cf_verbose,"","Couldn't open average database %s\n",VINPUTFILE);
   return;
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   MAX.Q[i].var = MAX.Q[i].expect = MAX.Q[i].q = 0.01;
   MIN.Q[i].var = MIN.Q[i].expect = MIN.Q[i].q = 9999.0;
   FPE[i] = FPQ[i] = NULL;
   }

for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
   {
   strcpy(timekey,GenTimeKey(now));

   if (ReadDB(dbp,timekey,&entry,sizeof(struct Averages)))
      {
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         if (fabs(entry.Q[i].expect) > MAX.Q[i].expect)
            {
            MAX.Q[i].expect = fabs(entry.Q[i].expect);
            }

         if (fabs(entry.Q[i].q) > MAX.Q[i].q)
            {
            MAX.Q[i].q = fabs(entry.Q[i].q);
            }

         if (fabs(entry.Q[i].expect) < MIN.Q[i].expect)
            {
            MIN.Q[i].expect = fabs(entry.Q[i].expect);
            }

         if (fabs(entry.Q[i].q) < MIN.Q[i].q)
            {
            MIN.Q[i].q = fabs(entry.Q[i].q);
            }
         }
      }
   }

CloseDB(dbp);
}

/****************************************************************************/

void EraseAverages()

{ int i;
 char timekey[CF_MAXVARSIZE],name[CF_MAXVARSIZE];
  struct Item *list = NULL;
  struct Averages entry;
  time_t now;
  CF_DB *dbp;

CfOut(cf_verbose,"","\nLooking through current database %s\n",VINPUTFILE);

list = SplitStringAsItemList(ERASE,',');

if (!OpenDB(VINPUTFILE,&dbp))
   {
   CfOut(cf_verbose,"","Couldn't open average database %s\n",VINPUTFILE);
   return;
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   FPE[i] = FPQ[i] = NULL;
   }

for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
   {
   strcpy(timekey,GenTimeKey(now));

   if (ReadDB(dbp,timekey,&entry,sizeof(struct Averages)))
      {
      for (i = 0; i < CF_OBSERVABLES; i++)
         {
         LookUpClassName(i,name);

         if (IsItemIn(list,name))
            {
            /* Set history but not most recent to zero */
            entry.Q[i].expect = 0;
            entry.Q[i].var = 0;
            entry.Q[i].q = 0;
            }
         }

      WriteDB(dbp,timekey,&entry,sizeof(struct Averages));
      }
   }

CloseDB(dbp);
}

/*****************************************************************************/

void SummarizeAverages()

{ int i;
  FILE *fout;
  char name[CF_BUFSIZE];
  CF_DB *dbp;

Banner("Summarizing monitor averages");

if (HTML)
   {
   snprintf(name,CF_BUFSIZE,"monitor_summary.html");
   }
else if (XML)
   {
   snprintf(name,CF_BUFSIZE,"monitor_summary.xml");
   }
else if (CSV)
   {
   snprintf(name,CF_BUFSIZE,"monitor_summary.csv");
   }
else
   {
   snprintf(name,CF_BUFSIZE,"monitor_summary.txt");
   }

if ((fout = fopen(name,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Unable to write to %s/%s\n",OUTPUTDIR,name);
   return;
   }

if (HTML && !EMBEDDED)
   {
   snprintf(name,CF_BUFSIZE,"Monitor summary for %s",VFQNAME);
   CfHtmlHeader(fout,name,STYLESHEET,WEBDRIVER,BANNER);
   fprintf(fout,"<table class=border cellpadding=5>\n");
   }
else if (XML)
   {
   fprintf(fout,"<?xml version=\"1.0\"?>\n<output>\n");
   }

CfOut(cf_inform,"","Writing report to %s\n",name);

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   LookUpClassName(i,name);

   if (XML)
      {
      fprintf(fout,"%s",CFRX[cfx_entry][cfb]);
      fprintf(fout,"%s %s %s",CFRX[cfx_event][cfb],name,CFRX[cfx_event][cfe]);
      fprintf(fout,"%s %.4lf %s",CFRX[cfx_min][cfb],MIN.Q[i].expect,CFRX[cfx_min][cfe]);
      fprintf(fout,"%s %.4lf %s",CFRX[cfx_max][cfb],MAX.Q[i].expect,CFRX[cfx_max][cfe]);
      fprintf(fout,"%s %.4lf %s",CFRX[cfx_dev][cfb],sqrt(MAX.Q[i].var),CFRX[cfx_dev][cfe]);
      fprintf(fout,"%s",CFRX[cfx_entry][cfe]);
      }
   else if (HTML)
      {
      fprintf(fout,"%s\n",CFRH[cfx_entry][cfb]);
      fprintf(fout,"%s %s %s\n",CFRH[cfx_event][cfb],name,CFRH[cfx_event][cfe]);
      fprintf(fout,"%s Min %.4lf %s\n",CFRH[cfx_min][cfb],MIN.Q[i].expect,CFRH[cfx_min][cfe]);
      fprintf(fout,"%s Max %.4lf %s\n",CFRH[cfx_max][cfb],MAX.Q[i].expect,CFRH[cfx_max][cfe]);
      fprintf(fout,"%s %.4lf %s\n",CFRH[cfx_dev][cfb],sqrt(MAX.Q[i].var),CFRH[cfx_dev][cfe]);
      fprintf(fout,"%s\n",CFRH[cfx_entry][cfe]);
      }
   else if (CSV)
      {
      fprintf(fout,"%2d,%-10s,%10lf,%10lf,%10lf\n",i,name,MIN.Q[i].expect,MAX.Q[i].expect,sqrt(MAX.Q[i].var));
      }
   else
      {
      fprintf(fout,"%2d. MAX <%-10s-in>   = %10lf - %10lf u %10lf\n",i,name,MIN.Q[i].expect,MAX.Q[i].expect,sqrt(MAX.Q[i].var));
      }
   }

if (!OpenDB(VINPUTFILE,&dbp))
   {
   CfOut(cf_error,"","Could not open %s",VINPUTFILE);
   return;
   }

if (ReadDB(dbp,"DATABASE_AGE",&AGE,sizeof(double)))
   {
   CfOut(cf_inform,""," ?? Database age is %.1f (weeks)\n\n",AGE/CF_WEEK*CF_MEASURE_INTERVAL);
   }

CloseDB(dbp);

if (HTML && !EMBEDDED)
   {
   fprintf(fout,"</table>");
   CfHtmlFooter(fout,FOOTER);
   }

if (XML)
   {
   fprintf(fout,"</output>\n");
   }

fclose(fout);
}

/*****************************************************************************/

void WriteGraphFiles()

{ int its,i,j,count = 0;
  double kept = 0, not_kept = 0, repaired = 0;
  struct Averages entry,det;
  char timekey[CF_MAXVARSIZE];
  time_t now;
  CF_DB *dbp;

CfOut(cf_verbose,""," -> Retrieving data from %s",VINPUTFILE);

if (!OpenDB(VINPUTFILE,&dbp))
   {
   CfOut(cf_verbose,"","Couldn't open average database %s\n",VINPUTFILE);
   return;
   }

OpenFiles();

if (TITLES)
   {
   char name[CF_MAXVARSIZE];

   for (i = 0; i < CF_OBSERVABLES; i+=2)
      {
      LookUpClassName(i,name);

      fprintf(FPAV,"# Column %d: %s\n",i,name);
      fprintf(FPVAR,"# Column %d: %s\n",i,name);
      fprintf(FPNOW,"# Column %d: %s\n",i,name);
      }

   fprintf(FPAV,"##############################################\n");
   fprintf(FPVAR,"##############################################\n");
   fprintf(FPNOW,"##############################################\n");
   }

if (HIRES)
   {
   its = 1;
   }
else
   {
   its = 12;
   }

now = CF_MONDAY_MORNING;

while (now < CF_MONDAY_MORNING + CF_WEEK)
   {
   memset(&entry,0,sizeof(entry));

   for (j = 0; j < its; j++)
      {
      strcpy(timekey,GenTimeKey(now));

      if (ReadDB(dbp,timekey,&det,sizeof(struct Averages)))
         {
         for (i = 0; i < CF_OBSERVABLES; i++)
            {
            entry.Q[i].expect += det.Q[i].expect/(double)its;
            entry.Q[i].var += det.Q[i].var/(double)its;
            entry.Q[i].q += det.Q[i].q/(double)its;
            }

         if (NOSCALING)
            {
            for (i = 1; i < CF_OBSERVABLES; i++)
               {
               MAX.Q[i].expect = 1;
               MAX.Q[i].q = 1;
               }
            }
         }

      now += CF_MEASURE_INTERVAL;
      count++;
      }

   /* Output the data in a plethora of files */

   fprintf(FPAV,"%d ",count);
   fprintf(FPVAR,"%d ",count);
   fprintf(FPNOW,"%d ",count);

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fprintf(FPAV,"%.4lf ",entry.Q[i].expect/MAX.Q[i].expect);
      fprintf(FPVAR,"%.4lf ",entry.Q[i].var/MAX.Q[i].var);
      fprintf(FPNOW,"%.4lf ",entry.Q[i].q/MAX.Q[i].q);

      if (entry.Q[i].q > entry.Q[i].expect + 2.0*sqrt(entry.Q[i].var))
         {
         not_kept++;
         continue;
         }

      if (entry.Q[i].q > entry.Q[i].expect + sqrt(entry.Q[i].var))
         {
         repaired++;
         continue;
         }

      if (entry.Q[i].q < entry.Q[i].expect - 2.0*sqrt(entry.Q[i].var))
         {
         not_kept++;
         continue;
         }

      if (entry.Q[i].q < entry.Q[i].expect - sqrt(entry.Q[i].var))
         {
         repaired++;
         continue;
         }

      kept++;
      }

   fprintf(FPAV,"\n");
   fprintf(FPVAR,"\n");
   fprintf(FPNOW,"\n");

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fprintf(FPE[i],"%d %.4lf %.4lf\n",count, entry.Q[i].expect, sqrt(entry.Q[i].var));
      /* Use same scaling for Q so graphs can be merged */
      fprintf(FPQ[i],"%d %.4lf 0.0\n",count, entry.Q[i].q);
      }
   }

METER_KEPT[meter_anomalies_day] = 100.0*kept/(kept+repaired+not_kept);
METER_REPAIRED[meter_anomalies_day] = 100.0*repaired/(kept+repaired+not_kept);

CloseDB(dbp);
CloseFiles();
}

/*****************************************************************************/

void MagnifyNow()

{ int its,i,j,count = 0;
  struct Averages entry,det;
  time_t now,here_and_now;
  char timekey[CF_MAXVARSIZE];
  CF_DB *dbp;

if (!OpenDB(VINPUTFILE,&dbp))
   {
   CfOut(cf_verbose,"","Couldn't open average database %s\n",VINPUTFILE);
   return;
   }

OpenMagnifyFiles();

its = 1; /* detailed view */

now = time(NULL);
here_and_now = now - (time_t)(4 * SECONDS_PER_HOUR);

while (here_and_now < now)
   {
   memset(&entry,0,sizeof(entry));

   for (j = 0; j < its; j++)
      {
      strcpy(timekey,GenTimeKey(here_and_now));

      if (ReadDB(dbp,timekey,&det,sizeof(struct Averages)))
         {
         for (i = 0; i < CF_OBSERVABLES; i++)
            {
            entry.Q[i].expect += det.Q[i].expect/(double)its;
            entry.Q[i].var += det.Q[i].var/(double)its;
            entry.Q[i].q += det.Q[i].q/(double)its;
            }

         if (NOSCALING)
            {
            for (i = 1; i < CF_OBSERVABLES; i++)
               {
               MAX.Q[i].expect = 1;
               MAX.Q[i].q = 1;
               }
            }
         }
      else
         {
         CfOut(cf_verbose,""," !! Read failed for ");
         }

      here_and_now += CF_MEASURE_INTERVAL;
      count++;
      }

   /* Output q and E/sig data in a plethora of files */

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      fprintf(FPM[i],"%d %.4lf %.4lf %.4lf\n",count, entry.Q[i].expect, sqrt(entry.Q[i].var),entry.Q[i].q);
      }
   }

CloseDB(dbp);
CloseMagnifyFiles();
}

/*****************************************************************************/

void WriteHistograms()

{ int i,j,k;
  int position,day;
  int weekly[CF_OBSERVABLES][CF_GRAINS];
  char filename[CF_BUFSIZE],name[CF_MAXVARSIZE];
  FILE *fp;

for (i = 0; i < 7; i++)
   {
   for (j = 0; j < CF_OBSERVABLES; j++)
      {
      for (k = 0; k < CF_GRAINS; k++)
         {
         HISTOGRAM[j][i][k] = 0;
         }
      }
    }

snprintf(filename,CF_BUFSIZE,"%s/state/histograms",CFWORKDIR);

if ((fp = fopen(filename,"r")) == NULL)
   {
   CfOut(cf_verbose,"","Unable to load histogram data\n");
   return;
   }

for (position = 0; position < CF_GRAINS; position++)
   {
   fscanf(fp,"%d ",&position);

   for (i = 0; i < CF_OBSERVABLES; i++)
      {
      for (day = 0; day < 7; day++)
         {
         fscanf(fp,"%d ",&(HISTOGRAM[i][day][position]));
         }

      weekly[i][position] = 0;
      }
   }

fclose(fp);

if (!HIRES)
   {
   /* Smooth daily and weekly histograms */
   for (k = 1; k < CF_GRAINS-1; k++)
      {
      for (j = 0; j < CF_OBSERVABLES; j++)
         {
         for (i = 0; i < 7; i++)
            {
            SMOOTHHISTOGRAM[j][i][k] = ((double)(HISTOGRAM[j][i][k-1] + HISTOGRAM[j][i][k] + HISTOGRAM[j][i][k+1]))/3.0;
            }
         }
      }
   }
else
   {
   for (k = 1; k < CF_GRAINS-1; k++)
      {
      for (j = 0; j < CF_OBSERVABLES; j++)
         {
         for (i = 0; i < 7; i++)
            {
            SMOOTHHISTOGRAM[j][i][k] = (double) HISTOGRAM[j][i][k];
            }
         }
      }
   }


for (i = 0; i < CF_OBSERVABLES; i++)
   {
   LookUpClassName(i,name);
   sprintf(filename,"%s.distr",name);

   if ((FPQ[i] = fopen(filename,"w")) == NULL)
      {
      CfOut(cf_inform,"fopen","Couldn't write %s",name);
      return;
      }
   }

/* Plot daily and weekly histograms */
for (k = 0; k < CF_GRAINS; k++)
   {
   int a;

   for (j = 0; j < CF_OBSERVABLES; j++)
      {
      for (i = 0; i < 7; i++)
         {
         weekly[j][k] += (int) (SMOOTHHISTOGRAM[j][i][k]+0.5);
         }
      }

   for (a = 0; a < CF_OBSERVABLES; a++)
      {
      fprintf(FPQ[a],"%d %d\n",k,weekly[a][k]);
      }
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   fclose(FPQ[i]);
   }
}

/*****************************************************************************/

void DiskArrivals(void)

{ CFDIR *dirh;
  FILE *fp;
  int count = 0, index = 0, i;
  char filename[CF_BUFSIZE],database[CF_BUFSIZE],timekey[CF_MAXVARSIZE];
  double val, maxval = 1.0, *array, grain = 0.0;
  time_t now;
  void *value;
  CF_DB *dbp = NULL;
  const struct dirent *dirp;

if ((array = (double *)malloc((int)CF_WEEK)) == NULL)
   {
   CfOut(cf_error,"MALLOC","Memory error");
   return;
   }

if ((dirh = OpenDirLocal(CFWORKDIR)) == NULL)
   {
   CfOut(cf_error,"opendir","Can't open directory %s\n",CFWORKDIR);
   free(array);
   return;
   }

CfOut(cf_verbose,"","\n\nLooking for filesystem arrival process data in %s\n",CFWORKDIR);

for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
   {
   if (strncmp(dirp->d_name,"scan:",5) == 0)
      {
      CfOut(cf_verbose,"","Found %s - generating X,Y plot\n",dirp->d_name);

      snprintf(database,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,dirp->d_name);

      if (!OpenDB(database,&dbp))
         {
         continue;
         }

      maxval = 1.0;
      grain = 0.0;
      count = 0.0;
      index = 0;

      for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
         {
         strcpy(timekey,GenTimeKey(now));

         if (ReadDB(dbp,timekey,&value,sizeof(struct Averages)))
            {
            grain += (double)*(double *)(value);
            }
         else
            {
            grain = 0;
            }

         if (HIRES)
            {
            if (grain > maxval)
               {
               maxval = grain;
               }

            array[index] = grain;
            grain = 0.0;
            index++;
            }
         else
            {
            if (count % 12 == 0)
               {
               if (grain > maxval)
                  {
                  maxval = grain;
                  }
               array[index] = grain;
               index++;
               grain = 0.0;
               }
            }
         count++;
         }

      CloseDB(dbp);

      snprintf(filename,CF_BUFSIZE-1,"%s.cfenv",dirp->d_name);

      if ((fp = fopen(filename,"w")) == NULL)
         {
         CfOut(cf_verbose,"fopen","Unable to open %s for writing\n",filename);
         CloseDir(dirh);
         free(array);
         return;
         }

      CfOut(cf_verbose,"","Data points = %d\n",index);

      for (i = 0; i < index; i++)
         {
         if (i > 1 && i < index-1)
            {
            val = (array[i-1]+array[i]+array[i+1])/3.0;  /* Smoothing */
            }
         else
            {
            val = array[i];
            }

         fprintf(fp,"%d %.4lf\n",i,val/maxval*50.0);
         }

      fclose(fp);
      }
   }

if (errno != 0)
   {
   CfOut(cf_error, "ReadDir", "Unable to traverse workdir");
   }

CloseDir(dirh);
free(array);
}

/***************************************************************/

void PeerIntermittency()

{ CF_DB *dbp;
  CF_DBC *dbcp;
  char *key;
  void *value;
  int ksize,vsize;
  FILE *fp1,*fp2;
  char name[CF_BUFSIZE],hostname[CF_BUFSIZE],timekey[CF_MAXVARSIZE];
  char out1[CF_BUFSIZE],out2[CF_BUFSIZE];
  struct QPoint entry;
  struct Item *ip, *hostlist = NULL;
  double average,var;
  time_t now = time(NULL), then, lastseen = CF_WEEK;

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

average = (double) CF_HOUR;  /* It will take a week for a host to be deemed reliable */
var = 0;

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan last-seen db");
   return;
   }


CfOut(cf_verbose,"","Examining known peers...\n");

while(NextDB(dbp,dbcp,&key,&ksize,&value,&vsize))
   {
   strcpy(hostname,(char *)key+1);

   if (!IsItemIn(hostlist,hostname))
      {
      /* Check hostname not recorded twice with +/- */
      AppendItem(&hostlist,hostname,NULL);
      CfOut(cf_verbose,"","Examining intermittent host %s\n",hostname);
      }
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);

/* Now go through each host and recompute entropy */

for (ip = hostlist; ip != NULL; ip=ip->next)
   {
   snprintf(out1,CF_BUFSIZE,"lastseen-%s.q",ip->name);

   CfOut(cf_verbose,"","Opening %s\n",out1);

   if ((fp1 = fopen(out1,"w")) == NULL)
      {
      CfOut(cf_verbose,"","Unable to open %s\n",out1);
      continue;
      }

   snprintf(out2,CF_BUFSIZE,"lastseen-%s.E-sigma",hostname);

   if ((fp2 = fopen(out2,"w")) == NULL)
      {
      CfOut(cf_verbose,"","Unable to open %s\n",out1);
      continue;
      }

   snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,ip->name);
   MapName(name);

   CfOut(cf_verbose,"","Consulting profile %s\n",name);

   if (!OpenDB(name,&dbp))
      {
      return;
      }

   for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING+CF_WEEK; now += CF_MEASURE_INTERVAL)
      {
      memset(&key,0,sizeof(key));
      memset(&value,0,sizeof(value));

      strcpy(timekey,GenTimeKey(now));

      if (ReadDB(dbp,timekey,&value,sizeof(entry)))
         {
         memcpy(&entry,value,sizeof(entry));
         then = (time_t)entry.q;
         lastseen = now - then;

         if (lastseen < 0)
            {
            lastseen = 0; /* Never seen before, so pretend */
            }

         average = (double)entry.expect;
         var = (double)entry.var;

         fprintf(fp2,"%d %lf %lf\n",now,average,sqrt(var));
         }
      else
         {
         /* If we have no data, it means no contact for whatever reason.
            It could be unable to respond unwilling to respond, policy etc.
            Assume for argument that we expect regular responses ... */

         lastseen += CF_MEASURE_INTERVAL; /* infer based on no data */
         }

      fprintf(fp1,"%d %d\n",now,lastseen);
      }

   fclose(fp1);
   fclose(fp2);
   CloseDB(dbp);
   }

DeleteItemList(hostlist);
}


/*********************************************************************/

void OpenFiles()

{ int i;
 char filename[CF_BUFSIZE],name[CF_MAXVARSIZE];

sprintf(filename,"cfenv-average");

if ((FPAV = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! File %s could not be opened for writing\n",filename);
   return;
   }

sprintf(filename,"cfenv-stddev");

if ((FPVAR = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! File %s could not be opened for writing\n",filename);
   return;
   }

sprintf(filename,"cfenv-now");

if ((FPNOW = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen"," !! File %s could not be opened for writing\n",filename);
   return;
   }

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   LookUpClassName(i,name);

   CfOut(cf_inform,""," -> Reporting on \"%s\"\n",name);

   sprintf(filename,"%s.E-sigma",name);

   if ((FPE[i] = fopen(filename,"w")) == NULL)
      {
      CfOut(cf_error,"fopen"," !! File %s could not be opened for writing\n",filename);
      return;
      }

   sprintf(filename,"%s.q",name);

   if ((FPQ[i] = fopen(filename,"w")) == NULL)
      {
      CfOut(cf_error,"fopen"," !! File %s could not be opened for writing\n",filename);
      return;
      }
   }
}

/*********************************************************************/

void CloseFiles()

{ int i;

fclose(FPAV);
fclose(FPVAR);
fclose(FPNOW);

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   fclose(FPE[i]);
   fclose(FPQ[i]);
   }
}

/*********************************************************************/

void OpenMagnifyFiles()

{ int i;
  char filename[CF_BUFSIZE],name[CF_MAXVARSIZE];

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   LookUpClassName(i,name);
   sprintf(filename,"%s.mag",name);

   CfOut(cf_inform,""," -> Magnifying \"%s\"\n",name);

   if ((FPM[i] = fopen(filename,"w")) == NULL)
      {
      perror("fopen");
      return;
      }
   }
}

/*********************************************************************/

void CloseMagnifyFiles()

{ int i;

for (i = 0; i < CF_OBSERVABLES; i++)
   {
   fclose(FPM[i]);
   }
}




/* EOF */




