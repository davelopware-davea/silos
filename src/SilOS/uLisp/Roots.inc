void silos_mark_roots () {
  for (std::size_t app_index = 0; app_index < AppCount; ++app_index) {
    markobject(reinterpret_cast<object *>(AppDeclarations[app_index].display_name));
    markobject(SilosActiveHandlers[app_index]);
    markobject(SilosAppStoreBindings[app_index].ref);
    markobject(SilosAppStoreBindings[app_index].watch);
    markobject(SilosAppStoreBindings[app_index].name);
    markobject(SilosAppStoreBindings[app_index].field_names);
  }
  silos_ui_visit_roots(markobject);
}

namespace {
void silos_move_root(object *&root, object *from, object *to) {
  if (root == from) root = to;
}
}

void silos_move_roots(object *from, object *to) {
  for (std::size_t app_index = 0; app_index < AppCount; ++app_index) {
    object *display_name = reinterpret_cast<object *>(
        AppDeclarations[app_index].display_name);
    silos_move_root(display_name, from, to);
    AppDeclarations[app_index].display_name = display_name;
    silos_move_root(SilosActiveHandlers[app_index], from, to);
    silos_move_root(SilosAppStoreBindings[app_index].ref, from, to);
    silos_move_root(SilosAppStoreBindings[app_index].watch, from, to);
    silos_move_root(SilosAppStoreBindings[app_index].name, from, to);
    silos_move_root(SilosAppStoreBindings[app_index].field_names, from, to);
  }
  silos_ui_move_roots(from, to);
}
