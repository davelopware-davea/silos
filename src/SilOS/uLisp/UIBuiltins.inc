const char doc_uitype[] = "(ui-type name (field type slot)...)";
const char doc_uibind[] = "(ui-bind name type)";
const char doc_uitemplate[] = "(ui-template name [(item type)] (ui-text string | (ui-field item-or-ui-ref field) :width integer :overflow ui-chop)...)";
const char doc_uitemplatelist[] = "(ui-template-list name :source ui-ref :item-template template :offset integer :limit integer :pending string :empty string :error string)";
const char doc_uimount[] = "(ui-mount template)";

namespace {
std::size_t current_ui_app() {
  if (CurrentAppIndex >= AppCount) error2("no current app");
  return CurrentAppIndex;
}
}

object *fn_uitype(object *args, object *env) {
  (void)env;
  return silos_ui_template_engine().defineType(current_ui_app(), args);
}

object *fn_uibind(object *args, object *env) {
  object *binding = findpair(first(args), env);
  if (binding == nil ||
      !silos_store_owns_ref(current_ui_app(), cdr(binding))) {
    error2("invalid ui source");
  }
  return silos_ui_template_engine().bind(
      current_ui_app(), args, env,
      cdr(binding));
}

object *fn_uitemplate(object *args, object *env) {
  return silos_ui_template_engine().defineTemplate(current_ui_app(), args,
                                                    env);
}

object *fn_uitemplatelist(object *args, object *env) {
  return silos_ui_template_engine().defineList(current_ui_app(), args, env);
}

object *fn_uimount(object *args, object *env) {
  (void)env;
  return silos_ui_template_engine().mount(current_ui_app(), args);
}
