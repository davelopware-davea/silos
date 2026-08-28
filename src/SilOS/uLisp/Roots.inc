void silos_mark_roots () {
  for (std::size_t app_index = 0; app_index < AppCount; ++app_index) {
    markobject(SilosActiveHandlers[app_index]);
    markobject(SilosAppStoreBindings[app_index].ref);
    markobject(SilosAppStoreBindings[app_index].watch);
    for (std::size_t ref_index = 0;
         ref_index < SilosAppUis[app_index].ref_count; ++ref_index) {
      markobject(SilosAppUis[app_index].refs[ref_index].binding);
    }
  }
}
