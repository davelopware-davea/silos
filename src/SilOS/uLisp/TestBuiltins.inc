// This is a test-only observation sink, not a proposed Store or Shell API.
// The Lisp watch has already read LIVE and OLD-VALUE; this hook just records
// that bounded proof data for the native CTest client without invoking the
// app-level handler.
object *fn_silostestwatchobservation (object *args, object *env) {
  (void) env;
  object *old_status = first(cdr(cdr(cdr(args))));
  if (!silos_symbol_is(first(args), "silos-ready") ||
      checkinteger(second(args)) != 2 ||
      !silos_symbol_is(old_status, "silos-pending")) {
    error2("unexpected StoreRef watch observation");
  }
  cstring(checkstring(third(args)), StoreRefWatchObservedDescription,
          sizeof(StoreRefWatchObservedDescription));
  ++StoreRefWatchObservationCount;
  StoreRefWatchObservedReady = true;
  StoreRefWatchObservedCount = true;
  StoreRefWatchObservedOldPending = true;
  return tee;
}

// Like the watch observation hook, this is only a compact CTest witness. The
// actual staging decision stays in the app's ordinary event handler.
object *fn_silostestappstage (object *args, object *env) {
  (void) env;
  const int stage = checkinteger(first(args));
  if (stage == 1 && !AppObservedStageOne) {
    AppObservedStageOne = true;
  } else if (stage == 2 && AppObservedStageOne && !AppObservedStageTwo) {
    AppObservedStageTwo = true;
    AppObservedPokeFifo = AppPokeCount == 2;
  } else {
    error2("unexpected app stage");
  }
  return tee;
}
