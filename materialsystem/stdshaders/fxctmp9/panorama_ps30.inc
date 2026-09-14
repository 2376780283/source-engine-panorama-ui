// SE port: hand-written stand-in for the file Valve's shader combo generator produces from
// panorama_ps30.fxc.  Static index is constant (no STATIC combos); the dynamic index packs the four
// DYNAMIC combos of the .fxc, in declaration order:
//
//   // DYNAMIC: "D_TEX2DFASTBLUR"  "0..8"
//   // DYNAMIC: "D_TEX2DBLUR"      "0..1"
//   // DYNAMIC: "D_TEX2DPARTICLE"  "0..1"
//   // DYNAMIC: "D_TEX2DDOWNSAMPLE" "0..1"
//
// Strides are the product of the preceding value counts (9, 2, 2, 2 -> 1, 9, 18, 36), matching the
// formula the s1wrapper PanDx path uses for the same shader in
// panorama/source2/renderer/source2surface.cpp.
#include "shaderlib/cshader.h"

class panorama_ps30_Static_Index
{
public:
	panorama_ps30_Static_Index() {}
	int GetIndex() { return 0; }
};
#define shaderStaticTest_panorama_ps30 0

class panorama_ps30_Dynamic_Index
{
private:
	int m_nD_TEX2DFASTBLUR;
	int m_nD_TEX2DBLUR;
	int m_nD_TEX2DPARTICLE;
	int m_nD_TEX2DDOWNSAMPLE;

public:
	void SetD_TEX2DFASTBLUR( int i )
	{
		Assert( i >= 0 && i <= 8 );
		m_nD_TEX2DFASTBLUR = i;
	}
	void SetD_TEX2DBLUR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DBLUR = i;
	}
	void SetD_TEX2DPARTICLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DPARTICLE = i;
	}
	void SetD_TEX2DDOWNSAMPLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DDOWNSAMPLE = i;
	}

	void SetD_TEX2DFASTBLUR( bool i ) { SetD_TEX2DFASTBLUR( i ? 1 : 0 ); }
	void SetD_TEX2DBLUR( bool i ) { SetD_TEX2DBLUR( i ? 1 : 0 ); }
	void SetD_TEX2DPARTICLE( bool i ) { SetD_TEX2DPARTICLE( i ? 1 : 0 ); }
	void SetD_TEX2DDOWNSAMPLE( bool i ) { SetD_TEX2DDOWNSAMPLE( i ? 1 : 0 ); }

public:
	panorama_ps30_Dynamic_Index()
	{
		m_nD_TEX2DFASTBLUR = 0;
		m_nD_TEX2DBLUR = 0;
		m_nD_TEX2DPARTICLE = 0;
		m_nD_TEX2DDOWNSAMPLE = 0;
	}

	int GetIndex()
	{
		return ( 1 * m_nD_TEX2DFASTBLUR ) + ( 9 * m_nD_TEX2DBLUR ) +
			   ( 18 * m_nD_TEX2DPARTICLE ) + ( 36 * m_nD_TEX2DDOWNSAMPLE );
	}
};
// The macro's value references the variables SET_DYNAMIC_PIXEL_SHADER_COMBO() declares, the same way
// the generated .inc files do (they are otherwise unused, which this module reports as an error).
#define shaderDynamicTest_panorama_ps30 ( psh_forgot_to_set_dynamic_D_TEX2DFASTBLUR + psh_forgot_to_set_dynamic_D_TEX2DBLUR + psh_forgot_to_set_dynamic_D_TEX2DPARTICLE + psh_forgot_to_set_dynamic_D_TEX2DDOWNSAMPLE )
