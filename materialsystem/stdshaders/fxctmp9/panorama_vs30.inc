// SE port: hand-written stand-in for the file Valve's shader combo generator produces from
// panorama_vs30.fxc.  That shader declares no STATIC/DYNAMIC combos (the .fxc has no "// STATIC:"
// or "// DYNAMIC:" lines), so both indices are constant zero.  The combo naming/format follows the
// generated .inc files next to this one.
#include "shaderlib/cshader.h"

class panorama_vs30_Static_Index
{
public:
	panorama_vs30_Static_Index() {}
	int GetIndex() { return 0; }
};
#define shaderStaticTest_panorama_vs30 0

class panorama_vs30_Dynamic_Index
{
public:
	panorama_vs30_Dynamic_Index() {}
	int GetIndex() { return 0; }
};
#define shaderDynamicTest_panorama_vs30 0
