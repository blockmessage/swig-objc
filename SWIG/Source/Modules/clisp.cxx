/* -----------------------------------------------------------------------------
 * clisp.cxx
 *
 *     clisp module.
 *
 * Author(s) : Surendra Singhi (surendra@asu.edu)
 *
 * Copyright (C) 2005 Surendra Singhi
 * See the file LICENSE for information on usage and redistribution.
 * ----------------------------------------------------------------------------- */

char cvsroot_clisp_cxx[] = "$Header$";

#include "swigmod.h"

class CLISP : public Language {
public:
  File *f_cl;
  String *module;
  virtual void main(int argc, char *argv[]);
  virtual int top(Node *n);
  virtual int functionWrapper(Node *n);
  virtual int variableWrapper(Node *n); 
  virtual int constantWrapper(Node *n);
  virtual int classDeclaration(Node *n);
  List *entries;
private:
  String* get_ffi_type(SwigType *ty);
  String* convert_literal(String *num_param, String *type);
  String* strip_parens(String *string);
  int generate_all_flag;
};

void CLISP :: main(int argc, char *argv[]) {
  int i;

  SWIG_library_directory("clisp"); 
  SWIG_config_file("clisp.swg");
  generate_all_flag = 0;
  
  for(i=1; i<argc; i++) {
    if (!strcmp(argv[i], "-help")) {
      Printf(stdout, "clisp Options (available with -clisp)\n");
      Printf(stdout, 
	     " -generate-all\n"
	     "\t If this option is given then clisp definitions for all the functions\n"
	     "and global variables will be created otherwise only definitions for \n"
	     "externed functions and variables is created.");
    }
    else if ( (Strcmp(argv[i],"-generate-all") == 0)) {
	    generate_all_flag = 1;
	    Swig_mark_arg(i);
    }
  }
}


int CLISP :: top(Node *n) {

  File *f_null=NewString("");
  module=Getattr(n, "name");
  String *output_filename;
  entries = NewList();
  
  /* Get the output file name */
  String *outfile = Getattr(n,"outfile");
    
  if(!outfile)
    output_filename=outfile;
  else  {
    output_filename=NewString("");
    Printf(output_filename, "%s%s.lisp", SWIG_output_directory(), module);
  }

  f_cl=NewFile(output_filename, "w+");
  if (!f_cl) {
    Printf(stderr, "Unable to open %s for writing\n", output_filename);
    SWIG_exit(EXIT_FAILURE);
  }

  Swig_register_filebyname("header",f_null);
  Swig_register_filebyname("runtime",f_null);
  Swig_register_filebyname("wrapper", f_null);


  String *header=NewStringf(";; This is an automatically generated file. \n;;Make changes as you feel are necessary (but remember if you try to regenerate this file, your changes will be lost). \n\n(defpackage \"%s\"\n  (:use :common-lisp :ffi)", module);

  Language::top(n);

  Iterator i;
  for (i = First(entries); i.item; i = Next(i)) {
    Printf(header,"\n\t:%s", i.item);
  }
  Printf(header, ")\n",NULL);

  Printf(header,"\n(in-package :%s)\n",module);

  long len= Tell(f_cl);

  Printf(f_cl,"%s",header);

  long end = Tell(f_cl);

  for(len--;len >=0 ; len --) {
    end--;
    Seek(f_cl,len,SEEK_SET);
    int ch=Getc(f_cl);
    Seek(f_cl,end,SEEK_SET);
    Putc(ch,f_cl);
  }
  
  Seek(f_cl,0,SEEK_SET);
  Write(f_cl,Char(header), Len(header));

  Close(f_cl);
  Delete(f_cl); // Deletes the handle, not the file

  return SWIG_OK;
}


int CLISP :: functionWrapper(Node *n) {

  String *storage=Getattr(n,"storage");
  if(!generate_all_flag && (!storage || (Strcmp(storage,"extern") && Strcmp(storage,"externc"))))
    return SWIG_OK;
     
  String *func_name=Getattr(n, "sym:name");

  ParmList *pl=Getattr(n, "parms");

  int argnum=0, first=1;

  Printf(f_cl, "\n(ffi:def-call-out %s\n\t(:name \"%s\")\n", func_name,func_name);
  
  Append(entries,func_name);
  /* Special cases */
  
  if (ParmList_len(pl) != 0) {
    Printf(f_cl, "\t(:arguments ");
  }
  for (Parm *p=pl; p; p=nextSibling(p), argnum++) {

    String *argname=Getattr(p, "name");
    SwigType *argtype=Getattr(p, "type");
    
    String *ffitype=get_ffi_type(argtype);
    
    int tempargname=0;
      
     if (!argname) {
       argname=NewStringf("arg%d", argnum);
       tempargname=1;
     }
      
     if (!first) {
       Printf(f_cl, "\n\t\t");
     }
     Printf(f_cl, "(%s %s)", argname, ffitype);
     first=0;
      
     Delete(ffitype);

     if (tempargname) 
       Delete(argname);
  }
  if (ParmList_len(pl) != 0) {
    Printf(f_cl, ")\n"); /* finish arg list */
  }
  Printf(f_cl, "\t(:return-type %s)\n", get_ffi_type(Getattr(n, "type")));
  Printf(f_cl, "\t(:library +library-name+))\n");
   
  return SWIG_OK;
}


int CLISP :: constantWrapper(Node *n) {
  String *type=Getattr(n, "type");
  String *converted_value=convert_literal(Getattr(n, "value"), type);
  String *name=Getattr(n, "sym:name");

#if 0
  Printf(stdout, "constant %s is of type %s. value: %s\n",
	 name, type, converted_value);
#endif

  Printf(f_cl, "\n(defconstant %s %s)\n",
	 name, converted_value);

  Delete(converted_value);
 
  return SWIG_OK;
}

int CLISP :: variableWrapper(Node *n) {
  String *storage=Getattr(n,"storage");
  if(!generate_all_flag && (!storage || (Strcmp(storage,"extern") && Strcmp(storage,"externc"))))
    return SWIG_OK;

  String *type=Getattr(n, "type");
  String *var_name=Getattr(n, "sym:name");
  String *lisp_type=get_ffi_type(type);
  Printf(f_cl,"\n(def-c-var %s (:type %s)\n",var_name,lisp_type);
  Printf(f_cl, "\t(:library +library-name+))\n");
  return SWIG_OK;
}


// Includes structs
int CLISP :: classDeclaration(Node *n) {
  String *name=Getattr(n, "sym:name");
  String *kind = Getattr(n,"kind");
  
  if (Strcmp(kind, "struct")) {
    Printf(stderr, "Don't know how to deal with %s kind of class yet.\n",
	   kind);
    Printf(stderr, " (name: %s)\n", name);
    SWIG_exit(EXIT_FAILURE);
  }

  Printf(f_cl,"\n(ffi:def-c-struct %s",name);

  Append(entries,NewStringf("make-%s",name));
	 
  for (Node *c=firstChild(n); c; c=nextSibling(c)) {
    String *lisp_type;

    if (Strcmp(nodeType(c), "cdecl")) {
      Printf(stderr, "Structure %s has a slot that we can't deal with.\n",
	     name);
      Printf(stderr, "nodeType: %s, name: %s, type: %s\n", 
	     nodeType(c),
	     Getattr(c, "name"),
	     Getattr(c, "type"));
      SWIG_exit(EXIT_FAILURE);
    }

    /* Printf(stdout, "Converting %s in %s\n", type, name); */
    String *temp=Copy(Getattr(c,"decl"));
    Append(temp,Getattr(c,"type"));
    lisp_type=get_ffi_type(temp);
    Delete(temp);

    String *slot_name = Getattr(c, "sym:name");
    Printf(f_cl, 
	   "\n\t(%s :type %s)", 
	   slot_name,
	   lisp_type);

    Append(entries,NewStringf("%s-%s",name,slot_name));
      
    Delete(lisp_type);
  }
  
  Printf(f_cl, ")\n");

  /* Add this structure to the known lisp types */
  //Printf(stdout, "Adding %s foreign type\n", name);
  //  add_defined_foreign_type(name);
  
  return SWIG_OK;
}

/* utilities */
/* returns new string w/ parens stripped */
String* CLISP::strip_parens(String *string) {
  char *s=Char(string), *p;
  int len=Len(string);
  String *res;
	
  if (len==0 || s[0] != '(' || s[len-1] != ')') {
    return NewString(string);
  }
	
  p=(char *)malloc(len-2+1);
  if (!p) {
    Printf(stderr, "Malloc failed\n");
    SWIG_exit(EXIT_FAILURE);
  }
	
  strncpy(p, s+1, len-1);
  p[len-2]=0; /* null terminate */
	
  res=NewString(p);
  free(p);
	
  return res;
}

String* CLISP::convert_literal(String *num_param, String *type) {
  String *num=strip_parens(num_param), *res;
  char *s=Char(num);
	
  /* Make sure doubles use 'd' instead of 'e' */
  if (!Strcmp(type, "double")) {
    String *updated=Copy(num);
    if (Replace(updated, "e", "d", DOH_REPLACE_ANY) > 1) {
      Printf(stderr, "Weird!! number %s looks invalid.\n", num);
      SWIG_exit(EXIT_FAILURE);
    }
    Delete(num);
    return updated;
  }

  if (SwigType_type(type) == T_CHAR) {
    /* Use CL syntax for character literals */
    return NewStringf("#\\%s", num_param);
  }
  else if (SwigType_type(type) == T_STRING) {
    /* Use CL syntax for string literals */
    return NewStringf("\"%s\"", num_param);
  }
	
  if (Len(num) < 2 || s[0] != '0') {
    return num;
  }
	
  /* octal or hex */
	
  res=NewStringf("#%c%s", 
		 s[1] == 'x' ? 'x' : 'o', 
		 s+2);
  Delete(num);

  return res;
}

String* CLISP::get_ffi_type(SwigType *ty) {
  Hash *typemap =Swig_typemap_search("in", ty,"", 0);
  if (typemap) {
    String *typespec = Getattr(typemap, "code");
    return NewString(typespec);
  }
  else if(SwigType_ispointer(ty)) {
    SwigType *cp = Copy(ty);
    SwigType_del_pointer(cp);
    String *inner_type=get_ffi_type(cp);
    String *str = NewStringf("(ffi:c-pointer %s)",inner_type);
    Delete(cp);
    Delete(inner_type);
    return str;
  }
  else if(SwigType_isarray(ty)) {
    SwigType *cp = Copy(ty);
    String *array_dim=SwigType_array_getdim(ty,0);

    if(!Strcmp(array_dim,""))  { //dimension less array convert to pointer
      Delete(array_dim);
      SwigType_del_array(cp);
      SwigType_add_pointer(cp);
      String *str =get_ffi_type(cp);
      Delete(cp);
      return str;
    }
    else {
      SwigType_pop_arrays(cp);
      String *inner_type = get_ffi_type(cp);
      Delete(cp);
	     
      int ndim=SwigType_array_ndim(ty);
      String *dimension;
      if(ndim == 1) {
	dimension=array_dim;
      }
      else {
	dimension = array_dim;
	for(int i=1;i<ndim;i++)	  {
	  array_dim=SwigType_array_getdim(ty,i);
	  Append(dimension," ");
	  Append(dimension,array_dim);
	  Delete(array_dim);
	}
	String *temp=dimension;
	dimension=NewStringf("(%s)",dimension);
	Delete(temp);
      }
      String *str=NewStringf("(ffi:c-array %s %s)",inner_type,dimension);
      Delete(inner_type);
      Delete(dimension);
      return str;
    }
  }
  String *str=SwigType_str(ty,0);
  if(str) {
    char *st = Strstr(str,"struct");
    if(st) {
      st+=7;
      return NewString(st);
    }
    char *cl = Strstr(str,"class");
    if(cl) {
      cl+=6;
      return NewString(cl);
    }
  }
  return str;
}

extern "C" Language *swig_clisp(void) {
  return new CLISP();
}

