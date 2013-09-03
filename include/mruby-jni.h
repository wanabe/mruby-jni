#include <jni.h>

mrb_value mrb_mruby_jni_wrap_jobject(mrb_state *mrb, struct RClass *klass, jobject jobj);
mrb_value mrb_mruby_jni_jclass2mclass(mrb_state *mrb, jobject jobj, mrb_value mobj);
int mrb_mruby_jni_check_exc(mrb_state *mrb);
struct RClass *mrb_mruby_jni_init(mrb_state *mrb);
