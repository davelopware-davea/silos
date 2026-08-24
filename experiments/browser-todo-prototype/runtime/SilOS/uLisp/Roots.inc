void silos_mark_roots () {
  markobject(SilosActiveHandler);
  markobject(SilosBoundStoreRef);
  markobject(SilosBoundStoreWatch);
  markobject(SilosUiSourceRef);
}
