// uLisp keeps its object model, evaluator, and GC internals private to its
// Arduino sketch. These interface-specific fragments deliberately share that
// translation unit; ordinary SilOS policy remains in compiled module sources.
#include "SilOS/uLisp/ULispAccess.h"
#include "SilOS/UI/IPlatformRenderEngine.h"
#include "SilOS/UI/UIAppBinding.h"
#include "SilOS/UI/UITemplateEngine.h"
#include "SilOS/uLisp/Common.inc"
#include "SilOS/uLisp/AppBuiltins.inc"
#include "SilOS/uLisp/StoreBuiltins.inc"
#include "SilOS/uLisp/UIBuiltins.inc"
#include "SilOS/uLisp/ShellBuiltins.inc"
#include "SilOS/uLisp/TestBuiltins.inc"
#include "SilOS/uLisp/Roots.inc"
