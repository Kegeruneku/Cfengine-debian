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
/* File: matching.c                                                          */
/*                                                                           */
/*****************************************************************************/
 
#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_PCRE_H
#include <pcre.h>
#endif

#ifdef HAVE_PCRE_PCRE_H
#include <pcre/pcre.h>
#endif

struct CfRegEx
{
   int failed;
   const char *regexp;
    pcre *rx;
    const char *err;
    int err_offset;
};

static int FullTextCaseMatch (char *regexp,const char *teststring);
static int BlockTextCaseMatch(char *regexp,char *teststring,int *start,int *end);

/*********************************************************************/
/* Wrappers                                                          */
/*********************************************************************/

static struct CfRegEx CompileRegExp(const char *regexp)

{ struct CfRegEx this;
 
 pcre *rx;
 const char *errorstr; 
 int erroffset;

memset(&this,0,sizeof(struct CfRegEx)); 

rx = pcre_compile(regexp,PCRE_MULTILINE|PCRE_DOTALL,&errorstr,&erroffset,NULL);

if (rx == NULL)
   {
   CfOut(cf_error,"","Regular expression error \"%s\" in expression \"%s\" at %d\n",errorstr,regexp,erroffset);
   this.failed = true;
   }
else
   {
   this.failed = false;
   this.rx = rx;
   }

this.regexp = regexp;
return this;
}

/*********************************************************************/

static struct CfRegEx CaseCompileRegExp(const char *regexp)

{ struct CfRegEx this;
 
 pcre *rx;
 const char *errorstr; 
 int erroffset;

memset(&this,0,sizeof(struct CfRegEx)); 
rx = pcre_compile(regexp,PCRE_CASELESS|PCRE_MULTILINE|PCRE_DOTALL,&errorstr,&erroffset,NULL);

if (rx == NULL)
   {
   CfOut(cf_error,"","Regular expression error %s in %s at %d: %s\n",errorstr,regexp,erroffset);
   this.failed = true;
   }
else
   {
   this.failed = false;
   this.rx = rx;
   }

this.regexp = regexp;
return this;
}

/*********************************************************************/

static int RegExMatchSubString(struct CfRegEx rex,char *teststring,int *start,int *end)

{
 pcre *rx;
 int ovector[OVECCOUNT],i,rc;
 
rx = rex.rx;

if ((rc = pcre_exec(rx,NULL,teststring,strlen(teststring),0,0,ovector,OVECCOUNT)) >= 0)
   {
   *start = ovector[0];
   *end = ovector[1];

   DeleteScope("match");
   NewScope("match");
   
   for (i = 0; i < rc; i++) /* make backref vars $(1),$(2) etc */
      {
      char substring[CF_MAXVARSIZE];
      char lval[4];
      char *backref_start = teststring + ovector[i*2];
      int backref_len = ovector[i*2+1] - ovector[i*2];

      if (backref_len < CF_MAXVARSIZE)
         {
         memset(substring,0,CF_MAXVARSIZE);
         strncpy(substring,backref_start,backref_len);
         snprintf(lval,3,"%d",i);
         ForceScalar(lval,substring);
         }
      }

   pcre_free(rx);
   return true;
   }
else
   {
   *start = 0;
   *end = 0;
   pcre_free(rx);
   return false;
   }
}

/*********************************************************************/

static int RegExMatchFullString(struct CfRegEx rex, const char *teststring)

{
 pcre *rx;
 int ovector[OVECCOUNT],i,rc,match_len;
 const char *match_start;
 
rx = rex.rx;

if ((rc = pcre_exec(rx,NULL,teststring,strlen(teststring),0,0,ovector,OVECCOUNT)) >= 0)
   {
   match_start = teststring + ovector[0];
   match_len = ovector[1] - ovector[0];

   DeleteScope("match");
   NewScope("match");
   
   for (i = 0; i < rc; i++) /* make backref vars $(1),$(2) etc */
      {
      char substring[CF_MAXVARSIZE];
      char lval[4];
      const char *backref_start = teststring + ovector[i*2];
      int backref_len = ovector[i*2+1] - ovector[i*2];

      memset(substring,0,CF_MAXVARSIZE);

      if (backref_len < CF_MAXVARSIZE)
         {
         strncpy(substring,backref_start,backref_len);
         snprintf(lval,3,"%d",i);
         ForceScalar(lval,substring);
         }
      }

   if (rx)
      {
      pcre_free(rx);
      }
      
   if ((match_start == teststring) && (match_len == strlen(teststring)))
      {
      return true;
      }
   else
      {
      return false;
      }
   }
else
   {
   pcre_free(rx);
   return false;
   }
}

/*********************************************************************/

static char *FirstBackReference(struct CfRegEx rex,char *regex, const char *teststring)

{ static char backreference[CF_BUFSIZE];

 pcre *rx;
 int ovector[OVECCOUNT],i,rc,match_len;
 const char *match_start;

rx = rex.rx;
memset(backreference,0,CF_BUFSIZE);

if ((rc = pcre_exec(rx,NULL,teststring,strlen(teststring),0,0,ovector,OVECCOUNT)) >= 0)
   {
   match_start = teststring + ovector[0];
   match_len = ovector[1] - ovector[0];

   for (i = 1; i < rc; i++) /* make backref vars $(1),$(2) etc */
      {
      const char *backref_start = teststring + ovector[i*2];
      int backref_len = ovector[i*2+1] - ovector[i*2];
      
      if (backref_len < CF_MAXVARSIZE)
         {
         strncpy(backreference,backref_start,backref_len);
         }

      break;
      }
   }

pcre_free(rx);
   
if (strlen(backreference) == 0)
   {
   Debug("The regular expression \"%s\" yielded no matching back-reference\n",regex);
   strncpy(backreference,"CF_NOMATCH",CF_MAXVARSIZE);
   }
else
   {
   Debug("The regular expression \"%s\" yielded backreference \"%s\" on %s\n",regex,backreference,teststring);
   }

return backreference;
}

bool ValidateRegEx(const char *regex)
{
    struct CfRegEx rex = CompileRegExp(regex);

    return !rex.failed;
}

/*************************************************************************/
/* WILDCARD TOOLKIT : Level 0                                            */
/*************************************************************************/

int FullTextMatch(char *regexp,const char *teststring)

{ struct CfRegEx rex;

if (strcmp(regexp,teststring) == 0)
   {
   return true;
   }
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   CfOut(cf_error,"","!! Could not parse regular expression '%s'", regexp);
   return false;
   }

if (RegExMatchFullString(rex,teststring))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*************************************************************************/

char *ExtractFirstReference(char *regexp, const char *teststring)
    
{ struct CfRegEx rex;
  static char *nothing = "";

if (regexp == NULL || teststring == NULL)
   {
   return nothing;
   }
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return nothing;
   }

return FirstBackReference(rex,regexp,teststring);
}

/*************************************************************************/

static int FullTextCaseMatch (char *regexp, const char *teststring)

{ struct CfRegEx rex;
 
rex = CaseCompileRegExp(regexp);

if (rex.failed)
   {
   return 0;
   }

if (RegExMatchFullString(rex,teststring))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*************************************************************************/

int BlockTextMatch(char *regexp,char *teststring,int *start,int *end)

{ struct CfRegEx rex;
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return 0;
   }

if (RegExMatchSubString(rex,teststring,start,end))
   {
   return true;
   }
else
   {
   return false;
   } 
}

/*************************************************************************/

static int BlockTextCaseMatch(char *regexp,char *teststring,int *start,int *end)

{ struct CfRegEx rex;
 
rex = CaseCompileRegExp(regexp);

if (rex.failed)
   {
   return 0;
   }

if (RegExMatchSubString(rex,teststring,start,end))
   {
   return true;
   }
else
   {
   return false;
   } 
}

/*********************************************************************/

int IsRegex(char *str)

{ char *sp;
  int ret = false;
  enum { r_norm, r_norepeat, r_literal } special = r_norepeat;
  int bracket = 0;
  int paren = 0;

/* Try to see when something is intended as a regular expression */
  
for (sp = str; *sp != '\0'; sp++)
   {
   if (special == r_literal)
      {
      special = r_norm;
      continue;
      }
   else if (*sp == '\\')
      {
      special = r_literal;
      continue;
      }
   else if (bracket && *sp != ']')
      {
      if (*sp == '[')
         {
         return false;
         }
      continue;
      }
 
   switch (*sp)
      {
      case '^':
	 special = (sp == str) ? r_norepeat : r_norm;
	 break;
      case '*':
      case '+':
	 if (special == r_norepeat)
            {
            return false;
            }
	 special = r_norepeat;
	 ret = true;
	 break;
      case '[':
	 special = r_norm;
	 bracket++;
	 ret = true;
	 break;
      case ']':
	 if (bracket == 0)
            {
            return false;
            }
	 bracket = 0;
	 special = r_norm;
	 break;
      case '(':
	 special = r_norepeat;
	 paren++;
	 break;

      case ')':
	 special = r_norm;
	 paren--;
	 if (paren < 0)
            {
            return false;
            }
	 break;
         
      case '|':
	 special = r_norepeat;
	 if (paren > 0)
            {
            ret = true;
            }
	 break;
         
      default:
	 special = r_norm;
      }

   }

if (bracket != 0 || paren != 0 || special == r_literal)
   {
   return false;
   }
else
   {
   return ret;
   }
}

/*********************************************************************/

int IsPathRegex(char *str)

{ char *sp;
  int result = false,s = 0,r = 0;

if ((result = IsRegex(str)))
   {
   for (sp = str; *sp != '\0'; sp++)
      {
      switch(*sp)
         {
         case '[':
             s++;
             break;
         case ']':             
             s--;
             if (s % 2 == 0)
                {
                result++;
                }
             break;
         case '(':
             r++;
             break;
         case')':
             r--;
             if (r % 2 == 0)
                {
                result++;
                }
             break;
         default:

             if (*sp == FILE_SEPARATOR && (r || s))
                {
                CfOut(cf_error,"","Path regular expression %s seems to use expressions containing the directory symbol %c",str,FILE_SEPARATOR);
                CfOut(cf_error,"","Use a work-around to avoid pathological behaviour\n");
                return false;
                }
             break;
         }
      }
   }

return result;
}

/*********************************************************************/

int IsRegexItemIn(struct Item *list,char *regex)

   /* Checks whether item matches a list of wildcards */

{ struct Item *ptr;

for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (ptr->classes && IsExcluded(ptr->classes))
      {
      continue;
      }

   /* Avoid using regex if possible, due to memory leak */
   
   if (strcmp(regex,ptr->name) == 0)
      {
      return(true);
      }

   /* Make it commutative */

   if (FullTextMatch(regex,ptr->name) || FullTextMatch(ptr->name,regex))
      {
      Debug("IsRegexItem(%s,%s)\n",regex,ptr->name);
      return(true);
      }
   }

return(false);
}

/*********************************************************************/

int MatchPolicy(char *camel,char *haystack,struct Attributes a,struct Promise *pp)

{ struct Rlist *rp;
  char *sp,*spto,*firstchar,*lastchar;
  enum insert_match opt;
  char work[CF_BUFSIZE],final[CF_BUFSIZE];
  struct Item *list = SplitString(camel,'\n'),*ip;
  int direct_cmp = false, ok = false;
  
//Split into separate lines first

for (ip = list; ip != NULL; ip = ip->next)
   {
   ok = false;
   direct_cmp = (strcmp(camel,haystack) == 0);
   
   if (a.insert_match == NULL) 
      {
      // No whitespace policy means exact_match
      ok = ok || direct_cmp;
      break;
      }
   
   memset(final,0,CF_BUFSIZE);
   strncpy(final,ip->name,CF_BUFSIZE-1);
   
   for (rp = a.insert_match; rp != NULL; rp=rp->next)
      {
      opt = String2InsertMatch(rp->item);
      
      /* Exact match can be done immediately */
      
      if (opt == cf_exact_match)
         {
         if (rp->next != NULL || rp != a.insert_match)
            {
            CfOut(cf_error,""," !! Multiple policies conflict with \"exact_match\", using exact match");
            PromiseRef(cf_error,pp);
            }

         ok = ok || direct_cmp;
         break;
         }
      
      if (opt == cf_ignore_embedded)
         {
         memset(work,0,CF_BUFSIZE);
         
         // Strip initial and final first

         for (firstchar = final; isspace(*firstchar); firstchar++)
            {
            }
         
         for (lastchar = final+strlen(final)-1; lastchar > firstchar && isspace(*lastchar); lastchar--)
            {
            }         

         for (sp = final,spto = work; *sp != '\0'; sp++)
            {
            if (sp > firstchar && sp < lastchar)
               {
               if (isspace(*sp))
                  {
                  while (isspace(*(sp+1)))
                     {
                     sp++;
                     }
                  
                  strcat(spto,"\\s+");
                  spto += 3;
                  }
               else
                  {
                  *spto++ = *sp;
                  }
               }
            else
               {
               *spto++ = *sp;
               }
            }
         
         strcpy(final,work);
         }
      
      if (opt == cf_ignore_leading)
         {
         if (strncmp(final,"\\s*",3) != 0)
            {
            for (sp = final; isspace(*sp); sp++)
               {
               }
            strcpy(work,sp);
            snprintf(final,CF_BUFSIZE,"\\s*%s",work);
            }
         }
      
      if (opt == cf_ignore_trailing)
         {
         if (strncmp(final+strlen(final)-4,"\\s*",3) != 0)
            {
            strcpy(work,final);
            snprintf(final,CF_BUFSIZE,"%s\\s*",work);
            }
         }

      ok = ok || FullTextMatch(final,haystack);
      }

   if (!ok) // All lines in region need to match to avoid insertions
      {
      break;
      }

   strcmp(final,work);
   }

DeleteItemList(list);
return ok;
}

/*********************************************************************/

int MatchRlistItem(struct Rlist *listofregex, const char *teststring)

   /* Checks whether item matches a list of wildcards */

{ struct Rlist *rp;
 
for (rp = listofregex; rp != NULL; rp=rp->next)
   {
   /* Avoid using regex if possible, due to memory leak */
   
   if (strcmp(teststring,rp->item) == 0)
      {
      return(true);
      }

   /* Make it commutative */
   
   if (FullTextMatch(rp->item,teststring))
      {
      Debug("MatchRlistItem(%s > %s)\n", (char *)rp->item, teststring);
      return true;
      }
   }

return false;
}


/*********************************************************************/
/* Enumerated languages - fuzzy match model                          */
/*********************************************************************/

int FuzzyMatchParse(char *s)

{ char *sp;
  short isCIDR = false, isrange = false, isv6 = false, isv4 = false, isADDR = false; 
  char address[CF_ADDRSIZE];
  int mask,count = 0;

Debug("Check ParsingIPRange(%s)\n",s);

for (sp = s; *sp != '\0'; sp++)  /* Is this an address or hostname */
   {
   if (!isxdigit((int)*sp))
      {
      isADDR = false;
      break;
      }
   
   if (*sp == ':')              /* Catches any ipv6 address */
      {
      isADDR = true;
      break;
      }

   if (isdigit((int)*sp))      /* catch non-ipv4 address - no more than 3 digits */
      {
      count++;
      if (count > 3)
         {
         isADDR = false;
         break;
         }
      }
   else
      {
      count = 0;
      }
   }
 
if (! isADDR)
   {
   return true;
   }
 
if (strstr(s,"/") != 0)
   {
   isCIDR = true;
   }

if (strstr(s,"-") != 0)
   {
   isrange = true;
   }

if (strstr(s,".") != 0)
   {
   isv4 = true;
   }

if (strstr(s,":") != 0)
   {
   isv6 = true;
   }

if (isv4 && isv6)
   {
   CfOut(cf_error,"","Mixture of IPv6 and IPv4 addresses");
   return false;
   }

if (isCIDR && isrange)
   {
   CfOut(cf_error,"","Cannot mix CIDR notation with xx-yy range notation");
   return false;
   }

if (isv4 && isCIDR)
   {
   if (strlen(s) > 4+3*4+1+2)  /* xxx.yyy.zzz.mmm/cc */
      {
      CfOut(cf_error,"","IPv4 address looks too long");
      return false;
      }
   
   address[0] = '\0';
   mask = 0;
   sscanf(s,"%16[^/]/%d",address,&mask);

   if (mask < 8)
      {
      CfOut(cf_error,"","Mask value %d in %s is less than 8",mask,s);
      return false;
      }

   if (mask > 30)
      {
      CfOut(cf_error,"","Mask value %d in %s is silly (> 30)",mask,s);
      return false;
      }
   }


if (isv4 && isrange)
   {
   long i, from = -1, to = -1;
   char *sp1,buffer1[CF_MAX_IP_LEN];
   
   sp1 = s;
   
   for (i = 0; i < 4; i++)
      {
      buffer1[0] = '\0';
      sscanf(sp1,"%[^.]",buffer1);
      sp1 += strlen(buffer1)+1;
      
      if (strstr(buffer1,"-"))
         {
         sscanf(buffer1,"%ld-%ld",&from,&to);
         
         if (from < 0 || to < 0)
            {
            CfOut(cf_error,"","Error in IP range - looks like address, or bad hostname");
            return false;
            }
         
         if (to < from)
            {
            CfOut(cf_error,"","Bad IP range");
            return false;
            }
         
         }
      }
   }
 
 if (isv6 && isCIDR)
    {
    char address[CF_ADDRSIZE];
    int mask,blocks;
    
    if (strlen(s) < 20)
       {
       CfOut(cf_error,"","IPv6 address looks too short");
       return false;
       }
    
    if (strlen(s) > 42)
       {
       CfOut(cf_error,"","IPv6 address looks too long");
       return false;
       }
    
    address[0] = '\0';
    mask = 0;
    sscanf(s,"%40[^/]/%d",address,&mask);
    blocks = mask/8;
    
    if (mask % 8 != 0)
       {
       CfOut(cf_error,"","Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
       return false;
       }
    
    if (mask > 15)
       {
       CfOut(cf_error,"","IPv6 CIDR mask is too large");
       return false;
       }
    }

return true; 
}

/*********************************************************************/

int FuzzySetMatch(char *s1,char *s2)

/* Match two IP strings - with : or . in hex or decimal
   s1 is the test string, and s2 is the reference e.g.
   FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0 */

{ short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
  char address[CF_ADDRSIZE];
  int mask;
  unsigned long a1,a2;

if (strcmp(s1,s2) == 0)
   {
   return 0;
   }
  
if (strstr(s1,"/") != 0)
   {
   isCIDR = true;
   }

if (strstr(s1,"-") != 0)
   {
   isrange = true;
   }

if (strstr(s1,".") != 0)
   {
   isv4 = true;
   }

if (strstr(s1,":") != 0)
   {
   isv6 = true;
   }

if (strstr(s2,".") != 0)
   {
   isv4 = true;
   }

if (strstr(s2,":") != 0)
   {
   isv6 = true;
   }

if (isv4 && isv6)
   {
   /* This is just wrong */
   return -1;
   }

if (isCIDR && isrange)
   {
   CfOut(cf_error,"","Cannot mix CIDR notation with xxx-yyy range notation: %s",s1);
   return -1;
   }

if (!(isv6 || isv4))
   {
   CfOut(cf_error,"","Not a valid address range - or not a fully qualified name: %s",s1);
   return -1;
   }

if (!(isrange||isCIDR)) 
   {
   if (strlen(s2) > strlen(s1))
      {
      if (*(s2+strlen(s1)) != '.')
         {
         return -1; // Because xxx.1 should not match xxx.12 in the same octet
         }
      }
   
   return strncmp(s1,s2,strlen(s1)); /* do partial string match */
   }
 
if (isv4)
   {
   if (isCIDR)
      {
      struct sockaddr_in addr1,addr2;
      int shift;

      address[0] = '\0';
      mask = 0;
      sscanf(s1,"%16[^/]/%d",address,&mask);
      shift = 32 - mask;

      sockaddr_pton(AF_INET, address, &addr1);
      sockaddr_pton(AF_INET, s2, &addr2);

      a1 = htonl(addr1.sin_addr.s_addr);
      a2 = htonl(addr2.sin_addr.s_addr);
      
      a1 = a1 >> shift;
      a2 = a2 >> shift;
      
      if (a1 == a2)
         {
         return 0;
         }
      else
         {
         return -1;
         }
      }
   else
      {
      long i, from = -1, to = -1, cmp = -1;
      char *sp1,*sp2,buffer1[CF_MAX_IP_LEN],buffer2[CF_MAX_IP_LEN];

      sp1 = s1;
      sp2 = s2;
      
      for (i = 0; i < 4; i++)
         {
         buffer1[0] = '\0';
         sscanf(sp1,"%[^.]",buffer1);
         
         if (strlen(buffer1) == 0)
            {
            break;
            }

         sp1 += strlen(buffer1)+1;
         sscanf(sp2,"%[^.]",buffer2);
         sp2 += strlen(buffer2)+1;
         
         if (strstr(buffer1,"-"))
            {
            sscanf(buffer1,"%ld-%ld",&from,&to);
            sscanf(buffer2,"%ld",&cmp);

            if (from < 0 || to < 0)
               {
               Debug("Couldn't read range\n");
               return -1;
               }
            
            if ((from > cmp) || (cmp > to))
               {
               Debug("Out of range %ld > %ld > %ld (range %s)\n",from,cmp,to,buffer2);
               return -1;
               }
            }
         else
            {
            sscanf(buffer1,"%ld",&from);
            sscanf(buffer2,"%ld",&cmp);
            
            if (from != cmp)
               {
               Debug("Unequal\n");
               return -1;
               }
            }
         
         Debug("Matched octet %s with %s\n",buffer1,buffer2);
         }
      
      Debug("Matched IP range\n");
      return 0;
      }
   }

#if defined(HAVE_GETADDRINFO)
if (isv6)
   {
   int i;

   if (isCIDR)
      {
      int blocks;
      struct sockaddr_in6 addr1,addr2;

      address[0] = '\0';
      mask = 0;
      sscanf(s1,"%40[^/]/%d",address,&mask);
      blocks = mask/8;

      if (mask % 8 != 0)
         {
         CfOut(cf_error,"","Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
         return -1;
         }

      sockaddr_pton(AF_INET6, address, &addr1);
      sockaddr_pton(AF_INET6, s2, &addr2);

      for (i = 0; i < blocks; i++) /* blocks < 16 */
         {
         if (addr1.sin6_addr.s6_addr[i] != addr2.sin6_addr.s6_addr[i])
            {
            return -1;
            }
         }
      return 0;
      }
   else
      {
      long i, from = -1, to = -1, cmp = -1;
      char *sp1,*sp2,buffer1[CF_MAX_IP_LEN],buffer2[CF_MAX_IP_LEN];

      sp1 = s1;
      sp2 = s2;
      
      for (i = 0; i < 8; i++)
         {
         sscanf(sp1,"%[^:]",buffer1);
         sp1 += strlen(buffer1)+1;
         sscanf(sp2,"%[^:]",buffer2);
         sp2 += strlen(buffer2)+1;
         
         if (strstr(buffer1,"-"))
            {
            sscanf(buffer1,"%lx-%lx",&from,&to);
            sscanf(buffer2,"%lx",&cmp);
            
            if (from < 0 || to < 0)
               {
               return -1;
               }
            
            if ((from >= cmp) || (cmp > to))
               {
               Debug("%lx < %lx < %lx\n",from,cmp,to);
               return -1;
               }
            }
         else
            {
            sscanf(buffer1,"%ld",&from);
            sscanf(buffer2,"%ld",&cmp);
            
            if (from != cmp)
               {
               return -1;
               }
            }
         }
      
      return 0;
      }
   }
#endif 

return -1; 
}

/*********************************************************************/

int FuzzyHostParse(char *arg1,char *arg2)

{
  long start = -1, end = -1, where = -1;
  int n;

n = sscanf(arg2,"%ld-%ld%n",&start,&end,&where);

if (n != 2)
   {
   CfOut(cf_error,"","HostRange syntax error: second arg should have X-Y format where X and Y are decimal numbers");
   return false;
   } 

return true; 
}

/*********************************************************************/

int FuzzyHostMatch(char *arg0, char* arg1, char *refhost)

{
  char *sp, refbase[CF_MAXVARSIZE];
  long cmp = -1, start = -1, end = -1;
  char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE];

strlcpy(refbase,refhost,CF_MAXVARSIZE);
sp = refbase + strlen(refbase) - 1;

while ( isdigit((int)*sp) )
   {
   sp--;
   }

sp++;
sscanf(sp,"%ld",&cmp);
*sp = '\0';

if (cmp < 0)
   {
   return 1;
   }

if (strlen(refbase) == 0)
   {
   return 1;
   }

sscanf(arg1,"%ld-%ld",&start,&end);

if ( cmp < start || cmp > end )
   {
   return 1;
   }

strncpy(buf1,ToLowerStr(refbase),CF_BUFSIZE-1);
strncpy(buf2,ToLowerStr(arg0),CF_BUFSIZE-1);

if (strcmp(buf1,buf2) != 0)
   {
   return 1;
   }

return 0;
}

/*********************************************************************/

void EscapeSpecialChars(char *str, char *strEsc, int strEscSz, char *noEsc)

/* Escapes non-alphanumeric chars, except sequence given in noEsc */

{ char *sp;
  int strEscPos = 0;
  
if (noEsc == NULL)
   {
   noEsc = "";
   }

memset(strEsc, 0, strEscSz);

for (sp = str; (*sp != '\0') && (strEscPos < strEscSz - 2); sp++)
   {
   if (strncmp(sp, noEsc, strlen(noEsc)) == 0)
      {
      if (strEscSz <= strEscPos + strlen(noEsc))
         {
         break;
         }
      
      strcat(strEsc, noEsc);
      strEscPos += strlen(noEsc);
      sp += strlen(noEsc);
      }
   
   if (*sp != '\0' && !isalnum(*sp))
      {
      strEsc[strEscPos++] = '\\';
      }
   
   strEsc[strEscPos++] = *sp;
   }
}

/*********************************************************************/

char *EscapeChar(char *str, int strSz, char esc)
/* Escapes characters esc in the string str of size strSz  */

{ char strDup[CF_BUFSIZE];
  int strPos, strDupPos;
  
if(sizeof(strDup) < strSz)
   {
   FatalError("Too large string passed to EscapeCharInplace()\n");
   }

snprintf(strDup, sizeof(strDup), "%s", str);
memset(str, 0, strSz);

for(strPos = 0, strDupPos = 0; strPos < strSz - 2; strPos++, strDupPos++)
   {
   if(strDup[strDupPos] == esc)
      {
      str[strPos] = '\\';
      strPos++;
      }
   
   str[strPos] = strDup[strDupPos];
   }

return str;
}

/*********************************************************************/

void AnchorRegex(char *regex, char *out, int outSz)

{
if (EMPTY(regex))
  {
  memset(out,0,outSz);
  }
else
  {
  snprintf(out,outSz,"^(%s)$",regex);
  }
}


/* EOF */
