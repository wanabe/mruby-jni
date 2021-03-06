#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"

int debug = 0;

static void jobj_free(mrb_state *mrb, void *p) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  if (p) {
    (*env)->DeleteGlobalRef(env, (jobject)p);
  }
}

static const struct mrb_data_type jobj_data_type = {
  "jobject", jobj_free,
};

static mrb_value jdefinition__set_class_path(mrb_state *mrb, mrb_value self) {
  mrb_value mobj, mpath;
  char *cpath;
  jclass jclazz, jglobal;
  JNIEnv* env = (JNIEnv*)mrb->ud;

  mrb_get_args(mrb, "o", &mpath);
  cpath = mrb_string_value_cstr(mrb, &mpath);
  jclazz = (*env)->FindClass(env, cpath);
  if ((*env)->ExceptionCheck(env)) {
    mrb_raisef(mrb, E_NAME_ERROR, "Jni: can't get %S", mpath);
  }

  jglobal = (*env)->NewGlobalRef(env, jclazz);
  mobj = mrb_obj_value(Data_Wrap_Struct(mrb, mrb->object_class, &jobj_data_type, (void*)jglobal));
  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb, "jclass"), mobj);

  (*env)->DeleteLocalRef(env, jclazz);
  return mpath;
}

int mrb_mruby_jni_check_jexc(mrb_state *mrb) {
  JNIEnv* env = (JNIEnv*)mrb->ud;

  if ((*env)->ExceptionCheck(env)) {
    return 1;
    //mrb_raisef(mrb, E_RUNTIME_ERROR, "Java error TODO: show more detail");
  }
  return 0;
}

mrb_value mrb_mruby_jni_wrap_jobject(mrb_state *mrb, struct RClass *klass, jobject jobj) {
  mrb_value mobj;
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jglobal;

  jglobal = (*env)->NewGlobalRef(env, jobj);
  mobj = mrb_obj_value(Data_Wrap_Struct(mrb, klass, &jobj_data_type, (void*)jglobal));
  (*env)->DeleteLocalRef(env, jobj);
  return mobj;
}

struct RJMethod;

typedef mrb_value (*caller_t)(mrb_state*, mrb_value, struct RJMethod*);

struct RJMethod {
  jmethodID id;
  caller_t caller;
  union opt1 {
    struct RClass *klass;
    struct RObject *obj;
  } opt1;
  union opt2 {
    int depth;
  } opt2;
  int argc;
  jvalue *argv;
  char *types;
};

static void jmeth_free(mrb_state *mrb, void *p) {
  struct RJMethod *smeth = (struct RJMethod *)p;
  if (smeth->argv) {
    free(smeth->argv);
  }
  if (smeth->types) {
    free(smeth->types);
  }
  free(p);
}

static const struct mrb_data_type jmeth_data_type = {
  "jmethod", jmeth_free,
};

static jarray jmeth_i__start_enum_ary(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth, mrb_value *pmary, int *psize) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jarray jary;

  jary = (jarray)(*env)->CallObjectMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  if (!jary) {
    return NULL;
  }
  if (rmeth->opt2.depth != 1) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "TODO: return nested array");
  }

  *psize = (*env)->GetArrayLength(env, jary);
  *pmary = mrb_ary_new_capa(mrb, *psize);
  return jary;
}

static mrb_value jmeth_i__call_void(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;

  (*env)->CallVoidMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return mobj;
}

static mrb_value jmeth_i__call_void_static(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jclass jclazz;

  jclazz = (jclass)DATA_PTR(mrb_iv_get(mrb, mobj, mrb_intern_cstr(mrb, "jclass")));
  (*env)->CallStaticVoidMethodA(env, jclazz, rmeth->id, rmeth->argv);
  return mobj;
}

static mrb_value jmeth_i__call_bool(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jboolean jb;

  jb = (*env)->CallBooleanMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return mrb_bool_value(jb);
}

static mrb_value jmeth_i__call_int(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jint ji;

  ji = (*env)->CallIntMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return mrb_fixnum_value(ji);
}

static mrb_value jmeth_i__call_int_ary(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jarray jary;
  mrb_value mitem, mary;
  int i, ai, size;
  jint *jints;

  jary = jmeth_i__start_enum_ary(mrb, mobj, rmeth, &mary, &size);
  if (!jary) {
    return mrb_nil_value();
  }
  jints = (*env)->GetIntArrayElements(env, jary, NULL);

  for (i = 0; i < size; i++) {
    ai = mrb_gc_arena_save(mrb);
    mitem = mrb_fixnum_value(jints[i]);
    mrb_ary_push(mrb, mary, mitem);
    mrb_gc_arena_restore(mrb, ai);
  }
  (*env)->ReleaseIntArrayElements(env, jary, jints, 0);
  (*env)->DeleteLocalRef(env, jary);
  return mary;
}

static mrb_value jmeth_i__call_int_static(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jint ji;
  jclass jclazz;

  jclazz = (jclass)DATA_PTR(mrb_iv_get(mrb, mobj, mrb_intern_cstr(mrb, "jclass")));
  ji = (*env)->CallStaticIntMethodA(env, jclazz, rmeth->id, rmeth->argv);
  return mrb_fixnum_value(ji);
}

static mrb_value jlong2mlong(mrb_state *mrb, jlong jl) {
  mrb_value mmod;
  mrb_value mlong[2];
  unsigned long long bits = (unsigned long long) jl;

  mmod = mrb_const_get(mrb, mrb_obj_value(mrb->object_class), mrb_intern_cstr(mrb, "Jni"));
  mmod = mrb_const_get(mrb, mmod, mrb_intern_cstr(mrb, "J"));
  mmod = mrb_const_get(mrb, mmod, mrb_intern_cstr(mrb, "Long"));
  mlong[0] = mrb_fixnum_value((int)((bits<<32)>>32));
  mlong[1] = mrb_fixnum_value((int)(bits>>32));
  return mrb_obj_new(mrb, mrb_class_ptr(mmod), 2, mlong);
}

static mrb_value jmeth_i__call_long(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jlong jl;

  jl = (*env)->CallLongMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return jlong2mlong(mrb, jl);
}

static mrb_value jmeth_i__call_long_ary(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jarray jary;
  mrb_value mitem, mary;
  int i, ai, size;
  jlong *jlongs;

  jary = jmeth_i__start_enum_ary(mrb, mobj, rmeth, &mary, &size);
  if (!jary) {
    return mrb_nil_value();
  }
  jlongs = (*env)->GetLongArrayElements(env, jary, NULL);

  for (i = 0; i < size; i++) {
    ai = mrb_gc_arena_save(mrb);
    mitem = jlong2mlong(mrb, jlongs[i]);
    mrb_ary_push(mrb, mary, mitem);
    mrb_gc_arena_restore(mrb, ai);
  }
  (*env)->ReleaseLongArrayElements(env, jary, jlongs, 0);
  (*env)->DeleteLocalRef(env, jary);
  return mary;
}

static mrb_value jmeth_i__call_float(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jfloat jf;

  jf = (*env)->CallFloatMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return mrb_float_value(mrb, jf);
}

static mrb_value jstr2mstr(mrb_state *mrb, jstring jstr) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  mrb_value mstr;
  jsize size;
  const char *cstr;

  size = (*env)->GetStringUTFLength(env, jstr);
  cstr = (*env)->GetStringUTFChars(env, jstr, NULL);
  mstr = mrb_str_new(mrb, cstr, size);

  (*env)->ReleaseStringUTFChars(env, jstr, cstr);
  (*env)->DeleteLocalRef(env, jstr);
  return mstr;
}

static mrb_value jmeth_i__call_str(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jstring jstr;

  jstr = (*env)->CallObjectMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  if (!jstr) {
    return mrb_nil_value();
  }
  return jstr2mstr(mrb, jstr);
}

static mrb_value jmeth_i__wrap_jclassobj(mrb_state *mrb, mrb_value mobj, jobject jobj) {
  mrb_value mclassclass;
  mclassclass = mrb_str_new(mrb, "java.lang.Class", 15);
  mclassclass = mrb_funcall(mrb, mobj, "name2class", 1, mclassclass);
  return mrb_mruby_jni_wrap_jobject(mrb, mrb_class_ptr(mclassclass), jobj);
}

mrb_value mrb_mruby_jni_jclass2mclass(mrb_state *mrb, jobject jobj, mrb_value mobj) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jstring jname;
  mrb_value mret, mclass, mname, mclassobj;
  const char *cname;
  jsize size;
  jclass jclazz;
  jmethodID jmeth;

  if (!jobj) {
    return mrb_nil_value();
  }
  jclazz = (*env)->GetObjectClass(env, jobj);
  jmeth = (*env)->GetMethodID(env, jclazz, "getName", "()Ljava/lang/String;");
  (*env)->DeleteLocalRef(env, jclazz);

  jname = (*env)->CallObjectMethod(env, jobj, jmeth);
  size = (*env)->GetStringUTFLength(env, jname);
  cname = (*env)->GetStringUTFChars(env, jname, NULL);
  mname = mrb_str_new(mrb, cname, size);
  (*env)->ReleaseStringUTFChars(env, jname, cname);
  (*env)->DeleteLocalRef(env, jname);

  mret = mclass = mrb_funcall(mrb, mobj, "name2class", 1, mname);
  while (mrb_type(mclass) == MRB_TT_ARRAY) {
    mclass = mrb_ary_ref(mrb, mclass, 0);
  }
  if (mrb_nil_p(mclass)) {
    return jmeth_i__wrap_jclassobj(mrb, mobj, jobj);
  }
  mclassobj = mrb_iv_get(mrb, mclass, mrb_intern_cstr(mrb, "@jclassobj"));
  if (mrb_nil_p(mclassobj)) {
    mclassobj = jmeth_i__wrap_jclassobj(mrb, mobj, jobj);
    mrb_iv_set(mrb, mclass, mrb_intern_cstr(mrb, "@jclassobj"), mclassobj);
  } else {
    (*env)->DeleteLocalRef(env, jobj);
  }
  return mret;
}

static mrb_value jmeth_i__call_class(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jobj;

  jobj = (*env)->CallObjectMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  return mrb_mruby_jni_jclass2mclass(mrb, jobj, mobj);
}

static mrb_value jmeth_i__call_class_ary(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jobj;
  jarray jary;
  mrb_value mitem, mary;
  int i, ai, size;

  jary = jmeth_i__start_enum_ary(mrb, mobj, rmeth, &mary, &size);
  if (!jary) {
    return mrb_nil_value();
  }

  for (i = 0; i < size; i++) {
    ai = mrb_gc_arena_save(mrb);
    jobj = (*env)->GetObjectArrayElement(env, jary, i);
    mitem = mrb_mruby_jni_jclass2mclass(mrb, jobj, mobj);
    mrb_ary_push(mrb, mary, mitem);
    mrb_gc_arena_restore(mrb, ai);
  }
  (*env)->DeleteLocalRef(env, jary);
  return mary;
}

static mrb_value jmeth_i__call_class_static(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jobj;
  jclass jclazz;

  jclazz = (jclass)DATA_PTR(mrb_iv_get(mrb, mobj, mrb_intern_cstr(mrb, "jclass")));
  jobj = (*env)->CallStaticObjectMethodA(env, jclazz, rmeth->id, rmeth->argv);
  if (!jobj) {
    return mrb_nil_value();
  }
  return mrb_mruby_jni_jclass2mclass(mrb, jobj, mobj);
}

static mrb_value jmeth_i__call_obj(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jobj;

  jobj = (*env)->CallObjectMethodA(env, (jobject)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  if (!jobj || mrb_mruby_jni_check_jexc(mrb)) {
    return mrb_nil_value();
  }
  return mrb_mruby_jni_wrap_jobject(mrb, rmeth->opt1.klass, jobj);
}

static mrb_value jmeth_i__call_obj_static(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobject jobj;

  mobj = mrb_iv_get(mrb, mobj, mrb_intern_cstr(mrb, "jclass"));
  jobj = (*env)->CallStaticObjectMethodA(env, (jclass)DATA_PTR(mobj), rmeth->id, rmeth->argv);
  if (!jobj) {
    return mrb_nil_value();
  }
  return mrb_mruby_jni_wrap_jobject(mrb, rmeth->opt1.klass, jobj);
}

static mrb_value jmeth_i__call_obj_ary(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jobjectArray jary;
  jobject jobj;
  mrb_value mitem, mary;
  int size, i;
  int ai;

  jary = jmeth_i__start_enum_ary(mrb, mobj, rmeth, &mary, &size);
  if (!jary) {
    return mrb_nil_value();
  }

  for (i = 0; i < size; i++) {
    ai = mrb_gc_arena_save(mrb);
    jobj = (*env)->GetObjectArrayElement(env, jary, i);
    mitem = mrb_mruby_jni_wrap_jobject(mrb, rmeth->opt1.klass, jobj);
    mrb_ary_push(mrb, mary, mitem);
    mrb_gc_arena_restore(mrb, ai);
  }
  (*env)->DeleteLocalRef(env, jary);
  return mary;
}

static mrb_value jmeth_i__call_constructor(mrb_state *mrb, mrb_value mobj, struct RJMethod *rmeth) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  jclass jclazz;
  jobject jobj;

  DATA_TYPE(mobj) = &jobj_data_type;
  DATA_PTR(mobj) = NULL;

  jclazz = DATA_PTR(mrb_obj_value(rmeth->opt1.obj));
  jobj = (*env)->NewObjectA(env, jclazz, rmeth->id, rmeth->argv);
  if (jobj) {
    DATA_PTR(mobj) = (*env)->NewGlobalRef(env, jobj);
    (*env)->DeleteLocalRef(env, jobj);
  } else {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "constructor returns null");
  }
  return mobj;
}

#define FLAG_STATIC (1 << 8)
#define FLAG_ARY (1 << 9)
#define CALLER_TYPE(c, s, d) ((c) | ((s) ? FLAG_STATIC : 0) | ((d) ? FLAG_ARY : 0))

static inline caller_t type2caller(const char *ctype, int is_static, int depth) {
  switch (CALLER_TYPE(ctype[0], is_static, depth)) {
    case 'V': {
      return jmeth_i__call_void;
    } break;
    case 'V' | FLAG_STATIC: {
      return jmeth_i__call_void_static;
    } break;
    case 'Z': {
      return jmeth_i__call_bool;
    } break;
    case 'I': {
      return jmeth_i__call_int;
    } break;
    case 'I' | FLAG_STATIC: {
      return jmeth_i__call_int_static;
    } break;
    case 'I' | FLAG_ARY: {
      return jmeth_i__call_int_ary;
    } break;
    case 'J': {
      return jmeth_i__call_long;
    } break;
    case 'J' | FLAG_ARY: {
      return jmeth_i__call_long_ary;
    } break;
    case 'F': {
      return jmeth_i__call_float;
    } break;
    case 's': {
      return jmeth_i__call_str;
    } break;
    case 'c': {
      return jmeth_i__call_class;
    } break;
    case 'c' | FLAG_ARY: {
      return jmeth_i__call_class_ary;
    } break;
    case 'c' | FLAG_STATIC: {
      return jmeth_i__call_class_static;
    } break;
    case 'L': {
      return jmeth_i__call_obj;
    } break;
    case 'L' | FLAG_STATIC: {
      return jmeth_i__call_obj_static;
    } break;
    case 'L' | FLAG_ARY: {
      return jmeth_i__call_obj_ary;
    } break;
  }
  return NULL;
}

static mrb_value jmeth__initialize(mrb_state *mrb, mrb_value self) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  mrb_value miclass, mclass, mname, mret, margs, msig;
  jclass jclazz;
  jmethodID jmeth;
  char *cname, *csig;
  struct RJMethod *smeth = (struct RJMethod *)malloc(sizeof(struct RJMethod));
  int is_static = 0;
  struct RArray *ary;

  DATA_TYPE(self) = &jmeth_data_type;
  DATA_PTR(self) = smeth;
  smeth->types = NULL;

  mrb_get_args(mrb, "oooo", &miclass, &mret, &mname, &margs);
  if (mrb_type(miclass) == MRB_TT_SCLASS) {
    is_static = 1;
    miclass = mrb_iv_get(mrb, miclass, mrb_intern_cstr(mrb, "__attached__"));
  }
  mclass = mrb_iv_get(mrb, miclass, mrb_intern_cstr(mrb, "jclass"));
  jclazz = DATA_PTR(mclass);
  cname = mrb_string_value_cstr(mrb, &mname);

  msig = mrb_funcall(mrb, miclass, "get_type", 1, margs);
  csig = mrb_string_value_cstr(mrb, &msig);
  smeth->types = (char*)malloc(RSTRING_LEN(msig) + 1);
  memcpy(smeth->types, csig, RSTRING_LEN(msig) + 1);

  msig = mrb_funcall(mrb, self, "get_sig", 2, mret, margs);
  csig = mrb_string_value_cstr(mrb, &msig);
  if (is_static) {
    jmeth = (*env)->GetStaticMethodID(env, jclazz, cname, csig);
  } else {
    jmeth = (*env)->GetMethodID(env, jclazz, cname, csig);
  }
  if ((*env)->ExceptionCheck(env)) {
    mrb_raisef(mrb, E_NAME_ERROR, "Jni: can't get %S%S", mname, msig);
  }

  ary = mrb_ary_ptr(margs);
  smeth->id = jmeth;
  if (cname[0] == '<') { /* <init> */
    smeth->opt1.obj = mrb_obj_ptr(mclass);
    smeth->caller = jmeth_i__call_constructor;
  } else {
    struct RClass *rmod = mrb_module_get(mrb, "Jni");
    int depth = 0;

    rmod = mrb_class_ptr(mrb_const_get(mrb, mrb_obj_value(rmod), mrb_intern_cstr(mrb, "Generics")));
    while (mrb_type(mret) == MRB_TT_ARRAY && RARRAY_LEN(mret)) {
      depth++;
      mret = *RARRAY_PTR(mret);
    }
    smeth->opt1.klass = mrb_class_ptr(mret);
    if (rmod == smeth->opt1.klass) { /* Generics */
      miclass = mrb_iv_get(mrb, miclass, mrb_intern_cstr(mrb, "@iclass"));
      smeth->opt1.klass = mrb_class_ptr(miclass);
    }

    msig = mrb_funcall(mrb, miclass, "class2type", 1, mret);
    csig = mrb_string_value_cstr(mrb, &msig);
    smeth->caller = type2caller(csig, is_static, depth);
    smeth->opt2.depth = depth;
    if (!smeth->caller) {
      mrb_value mstatic = mrb_str_new_cstr(mrb, is_static ? "static " : "");
      mrb_value misary = mrb_str_new_cstr(mrb, depth ? "array " : "");
      mrb_raisef(mrb, E_RUNTIME_ERROR, "Jni: unsupported return type: %S%S%S", mstatic, misary, msig);
    }
  }
  smeth->argc = ary->len;
  smeth->argv = (jvalue *)malloc(ary->len * sizeof(jvalue));
  DATA_TYPE(self) = &jmeth_data_type;
  DATA_PTR(self) = smeth;

  return self;
}

static mrb_value jmeth__types(mrb_state *mrb, mrb_value self) {
  struct RJMethod *smeth = DATA_PTR(self);

  return mrb_str_new_cstr(mrb, smeth->types);
}

#define TYPE_VAL(c, mtype) (((int)(unsigned char)c) | ((mtype) << 8))

static char *mobj2jvalue(mrb_state *mrb, char *types, mrb_value mobj, jvalue *jval) {
  JNIEnv* env = (JNIEnv*)mrb->ud;

  switch (TYPE_VAL(*types++, mrb_type(mobj))) {
    case TYPE_VAL('Z', MRB_TT_FALSE):
    case TYPE_VAL('Z', MRB_TT_TRUE): {
      if (jval) {
        jval->z = mrb_bool(mobj);
      }
    } break;
    case TYPE_VAL('I', MRB_TT_FIXNUM): {
      if (jval) {
        jval->i = mrb_fixnum(mobj);
      }
    } break;
    case TYPE_VAL('F', MRB_TT_FLOAT): {
      if (jval) {
        jval->f = mrb_float(mobj);
      }
    } break;
    case TYPE_VAL('F', MRB_TT_FIXNUM): {
      if (jval) {
        jval->f = mrb_fixnum(mobj);
      }
    } break;
    case TYPE_VAL('L', MRB_TT_STRING):
    case TYPE_VAL('s', MRB_TT_STRING): {
      if (jval) {
        jval->l = (jobject)(*env)->NewStringUTF(env, mrb_string_value_cstr(mrb, &mobj));
      }
    } break;
    case TYPE_VAL('L', MRB_TT_FALSE): {
      types = strchr(types, ';') + 1;
      if (jval) {
        jval->l = (jobject)NULL;
      }
    } break;
    case TYPE_VAL('L', MRB_TT_DATA): {
      char *cname = types;
      mrb_value mclass;

      types = strchr(types, ';');
      mclass = mrb_str_new(mrb, cname, (types++) - cname);
      mclass = mrb_funcall(mrb, mobj, "name2class", 1, mclass);
      if (mrb_nil_p(mclass)) {
        return NULL;
      }
      if (!mrb_obj_is_kind_of(mrb, mobj, mrb_class_ptr(mclass))) {
        return NULL;
      }
      if (jval) {
        jval->l = (jobject)DATA_PTR(mobj);
      }
    } break;
    case TYPE_VAL('[', MRB_TT_ARRAY): {
      struct RArray *ary;
      int i, size, len;
      jarray jary = NULL;
      char *head, *ptr = NULL;

      head = types;
      ary = mrb_ary_ptr(mobj);
      len = ary->len;
      switch (*head) {
        case 'F': {
          if (jval) {
            jary = (*env)->NewFloatArray(env, len);
            size = sizeof(jfloat);
            ptr = (char*)(*env)->GetFloatArrayElements(env, jary, NULL);
          }
        } break;
        default: {
          return NULL;
        }
      }
      for (i = 0; i < len; i++) {
        mrb_value mitem = ary->ptr[i];
        jvalue *jarg = NULL;

        if (jval) {
          jarg = (jvalue*)(ptr + i * size);
        }
        types = mobj2jvalue(mrb, head, mitem, jarg);
        if (!types) {
          return NULL;
        }
      }
      switch (*head) {
        case 'F': {
          if (jval) {
            (*env)->ReleaseFloatArrayElements(env, jary, (jfloat*)ptr, 0);
          }
        } break;
        default: {
          return NULL;
        }
      }
      if (jval) {
        jval->l = jary;
      }
    } break;
    default: {
      return NULL;
    }
  }

  return types;
}

static mrb_value jmeth__check(mrb_state *mrb, mrb_value self) {
  mrb_value margs;
  struct RJMethod *smeth = DATA_PTR(self);
  struct RArray *ary;
  int i;
  char *types = smeth->types;

  mrb_get_args(mrb, "o", &margs);
  ary = mrb_ary_ptr(margs);
  if (ary->len != smeth->argc) {
    return mrb_false_value();
  }
  for (i = 0; i < ary->len; i++) {
    smeth->argv[i].i = 0;
  }
  for (i = 0; i < ary->len; i++) {
    mrb_value item = ary->ptr[i];

    types = mobj2jvalue(mrb, types, item, NULL);
    if (!types) {
      return mrb_false_value();
    }
  }
  return mrb_true_value();
}

static mrb_value jmeth__call(mrb_state *mrb, mrb_value self) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  mrb_value mobj, mname, margs;
  struct RJMethod *smeth;
  int i;
  char *types;
  struct RArray *ary;

  smeth = DATA_PTR(self);
  mrb_get_args(mrb, "ooo", &mobj, &mname, &margs);
  ary = mrb_ary_ptr(margs);
  types = smeth->types;

  for (i = 0; i < ary->len; i++) {
    mrb_value item = ary->ptr[i];
    jvalue *jarg = smeth->argv + i;

    types = mobj2jvalue(mrb, types, item, jarg);
    if (!types) {
      return mrb_false_value();
    }
  }
  mobj = smeth->caller(mrb, mobj, smeth);
  for (i = 0; i < smeth->argc; i++) {
    mrb_value mitem = ary->ptr[i];
    jvalue *jarg = smeth->argv + i;
    switch (mrb_type(mitem)) {
      case MRB_TT_STRING: {
        (*env)->DeleteLocalRef(env, jarg->l);
      } break;
    }
  }
  if ((*env)->ExceptionCheck(env)) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "exception in java method '%S'", mname);
  }
  return mobj;
}

static mrb_value jni_s__set_class_path(mrb_state *mrb, mrb_value self) {
  mrb_value mmod, mpath;
  mrb_get_args(mrb, "oo", &mmod, &mpath);
  mrb_iv_set(mrb, mmod, mrb_intern_cstr(mrb, "__classpath__"), mpath);
  return mrb_nil_value();
}

static mrb_value jni_s__get_field_static(mrb_state *mrb, mrb_value self) {
  JNIEnv* env = (JNIEnv*)mrb->ud;
  mrb_value mmod, mstr, mclass, mret, mname;
  jfieldID fid;
  jclass jclazz;
  char *cname, *cstr;

  mmod = mrb_const_get(mrb, self, mrb_intern_cstr(mrb, "Object"));
  mrb_get_args(mrb, "ooo", &mclass, &mret, &mname);
  jclazz = DATA_PTR(mrb_iv_get(mrb, mclass, mrb_intern_cstr(mrb, "jclass")));

  cname = mrb_string_value_cstr(mrb, &mname);
  mstr = mrb_funcall(mrb, mmod, "class2sig", 1, mret);
  cstr = mrb_string_value_cstr(mrb, &mstr);
  fid = (*env)->GetStaticFieldID(env, jclazz, cname, cstr);
  if ((*env)->ExceptionCheck(env)) {
    mrb_raisef(mrb, E_NAME_ERROR, "Jni: can't get field %S", mstr);
  }

  mstr = mrb_funcall(mrb, mmod, "class2type", 1, mret);
  cstr = mrb_string_value_cstr(mrb, &mstr);
  switch (cstr[0]) {
    case 'Z': {
      jboolean jval;
      jval = (*env)->GetStaticBooleanField(env, jclazz, fid);
      return jval ? mrb_true_value() : mrb_false_value();
    } break;
    case 'I': {
      jint jval;
      jval = (*env)->GetStaticIntField(env, jclazz, fid);
      return mrb_fixnum_value(jval);
    } break;
    case 'J': {
      jlong jval = (*env)->GetStaticLongField(env, jclazz, fid);
      return jlong2mlong(mrb, jval);
    }
    case 'L': {
      jobject jval;
      jval = (*env)->GetStaticObjectField(env, jclazz, fid);
      return mrb_mruby_jni_wrap_jobject(mrb, mrb_class_ptr(mret), jval);
    } break;
    case 's': {
      jstring jstr;
      jstr = (*env)->GetStaticObjectField(env, jclazz, fid);
      return jstr2mstr(mrb, jstr);
    } break;
  }
  mclass = mrb_funcall(mrb, mclass, "inspect", 0);
  mrb_raisef(mrb, E_RUNTIME_ERROR, "unsupported field: %S (%S in %S)", mstr, mname, mclass);
  return mrb_nil_value();
}

static mrb_value jni_s__debug(mrb_state *mrb, mrb_value self) {
  debug = !debug;
  return mrb_nil_value();
}

static mrb_value jni_s__clear_exception(mrb_state *mrb, mrb_value self) {
  JNIEnv* env = (JNIEnv*)mrb->ud;

  (*env)->ExceptionClear(env);
  return mrb_nil_value();
}

struct RClass *mrb_mruby_jni_init(mrb_state *mrb) {
  struct RClass *klass, *mod;
  mod = mrb_define_module(mrb,
    "Jni");
  mrb_define_singleton_method(mrb, (struct RObject *)mod, "set_classpath", jni_s__set_class_path, ARGS_REQ(2));
  mrb_define_singleton_method(mrb, (struct RObject *)mod, "get_field_static", jni_s__get_field_static, ARGS_REQ(3));
  mrb_define_singleton_method(mrb, (struct RObject *)mod, "debug", jni_s__debug, ARGS_NONE());
  mrb_define_singleton_method(mrb, (struct RObject *)mod, "clear_exception", jni_s__clear_exception, ARGS_NONE());

  klass = mrb_define_module_under(mrb, mod,
    "Definition");
  mrb_define_method(mrb, klass, "class_path=", jdefinition__set_class_path, ARGS_REQ(1));

  klass = mrb_define_class_under(mrb, mod,
    "Method", mrb->object_class);
  MRB_SET_INSTANCE_TT(klass, MRB_TT_DATA);
  mrb_define_method(mrb, klass, "initialize", jmeth__initialize, ARGS_REQ(3));
  mrb_define_method(mrb, klass, "types", jmeth__types, ARGS_REQ(0));
  mrb_define_method(mrb, klass, "check", jmeth__check, ARGS_REQ(1));
  mrb_define_method(mrb, klass, "call", jmeth__call, ARGS_REQ(3));

  klass = mrb_define_class_under(mrb, mod,
    "Object", mrb->object_class);
  MRB_SET_INSTANCE_TT(klass, MRB_TT_DATA);

  return mod;
}

int mrb_mruby_jni_check_exc(mrb_state *mrb) {
  JNIEnv* env = (JNIEnv*)mrb->ud;

  if (mrb->exc) {
    mrb_value mstr;
    mrb_value mexc;
    mrb_value mback;
    jclass jclazz;
    if ((*env)->ExceptionCheck(env)) {
      //(*env)->ExceptionClear(env);
      mrb->exc = 0;
      return 1;
    }
    mexc =  mrb_obj_value(mrb->exc);
    mstr = mrb_funcall(mrb, mexc, "message", 0);
    mrb_funcall(mrb, mstr, "<<", 1, mrb_str_new(mrb, "\n", 1));
    mback = mrb_funcall(mrb, mexc, "backtrace", 0);
    mback = mrb_funcall(mrb, mback, "join", 1, mrb_str_new(mrb, "\n", 1));
    mrb_funcall(mrb, mstr, "<<", 1, mback);
    mrb->exc = 0;
    jclazz = (*env)->FindClass(env, "java/lang/RuntimeException");
    if (jclazz) {
      (*env)->ThrowNew(env, jclazz, mrb_string_value_cstr(mrb, &mstr));
      (*env)->DeleteLocalRef(env, jclazz);
    }
    return 1;
  }
  return 0;
}

void mrb_mruby_jni_gem_init(mrb_state* mrb) {}
void mrb_mruby_jni_gem_final(mrb_state* mrb) {}
