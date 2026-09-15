// SE port: hand-written stand-in for the file Valve's shader combo generator produces from
// panoramafancy_ps30.fxc.  The .fxc declares ten DYNAMIC combos and no STATIC ones; the dynamic
// index packs them in declaration order with strides equal to the product of the preceding value
// counts:
//
//   // DYNAMIC: "D_TEXTURETYPE"        "0..4"   stride 1
//   // DYNAMIC: "D_PREMULTIPLY_ALPHA"  "0..1"   stride 5
//   // DYNAMIC: "D_USERADIALGRADIENT"  "0..1"   stride 10
//   // DYNAMIC: "D_GRADIENT_TWOSTOP"   "0..1"   stride 20
//   // DYNAMIC: "D_GRADIENT_COMPLEX"   "0..1"   stride 40
//   // DYNAMIC: "D_USEOUTERCORNER"     "0..1"   stride 80
//   // DYNAMIC: "D_USEINNERCORNER"     "0..1"   stride 160
//   // DYNAMIC: "D_COLORCORRECTION"    "0..1"   stride 320
//   // DYNAMIC: "D_USEOPACITYMASK"     "0..1"   stride 640
//   // DYNAMIC: "D_USERADIALCLIP"      "0..1"   stride 1280
//
// This matches the combo formula the s1wrapper PanDx path uses for the same shader in
// panorama/source2/renderer/source2surface.cpp.
#include "shaderlib/cshader.h"

class panoramafancy_ps30_Static_Index
{
public:
	panoramafancy_ps30_Static_Index() {}
	int GetIndex() { return 0; }
};
#define shaderStaticTest_panoramafancy_ps30 0

class panoramafancy_ps30_Dynamic_Index
{
private:
	int m_nD_TEXTURETYPE;
	int m_nD_PREMULTIPLY_ALPHA;
	int m_nD_USERADIALGRADIENT;
	int m_nD_GRADIENT_TWOSTOP;
	int m_nD_GRADIENT_COMPLEX;
	int m_nD_USEOUTERCORNER;
	int m_nD_USEINNERCORNER;
	int m_nD_COLORCORRECTION;
	int m_nD_USEOPACITYMASK;
	int m_nD_USERADIALCLIP;

public:
	void SetD_TEXTURETYPE( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nD_TEXTURETYPE = i;
	}
	void SetD_PREMULTIPLY_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_PREMULTIPLY_ALPHA = i;
	}
	void SetD_USERADIALGRADIENT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USERADIALGRADIENT = i;
	}
	void SetD_GRADIENT_TWOSTOP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_GRADIENT_TWOSTOP = i;
	}
	void SetD_GRADIENT_COMPLEX( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_GRADIENT_COMPLEX = i;
	}
	void SetD_USEOUTERCORNER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEOUTERCORNER = i;
	}
	void SetD_USEINNERCORNER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEINNERCORNER = i;
	}
	void SetD_COLORCORRECTION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_COLORCORRECTION = i;
	}
	void SetD_USEOPACITYMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEOPACITYMASK = i;
	}
	void SetD_USERADIALCLIP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USERADIALCLIP = i;
	}

	void SetD_TEXTURETYPE( bool i ) { SetD_TEXTURETYPE( i ? 1 : 0 ); }
	void SetD_PREMULTIPLY_ALPHA( bool i ) { SetD_PREMULTIPLY_ALPHA( i ? 1 : 0 ); }
	void SetD_USERADIALGRADIENT( bool i ) { SetD_USERADIALGRADIENT( i ? 1 : 0 ); }
	void SetD_GRADIENT_TWOSTOP( bool i ) { SetD_GRADIENT_TWOSTOP( i ? 1 : 0 ); }
	void SetD_GRADIENT_COMPLEX( bool i ) { SetD_GRADIENT_COMPLEX( i ? 1 : 0 ); }
	void SetD_USEOUTERCORNER( bool i ) { SetD_USEOUTERCORNER( i ? 1 : 0 ); }
	void SetD_USEINNERCORNER( bool i ) { SetD_USEINNERCORNER( i ? 1 : 0 ); }
	void SetD_COLORCORRECTION( bool i ) { SetD_COLORCORRECTION( i ? 1 : 0 ); }
	void SetD_USEOPACITYMASK( bool i ) { SetD_USEOPACITYMASK( i ? 1 : 0 ); }
	void SetD_USERADIALCLIP( bool i ) { SetD_USERADIALCLIP( i ? 1 : 0 ); }

public:
	panoramafancy_ps30_Dynamic_Index()
	{
		m_nD_TEXTURETYPE = 0;
		m_nD_PREMULTIPLY_ALPHA = 0;
		m_nD_USERADIALGRADIENT = 0;
		m_nD_GRADIENT_TWOSTOP = 0;
		m_nD_GRADIENT_COMPLEX = 0;
		m_nD_USEOUTERCORNER = 0;
		m_nD_USEINNERCORNER = 0;
		m_nD_COLORCORRECTION = 0;
		m_nD_USEOPACITYMASK = 0;
		m_nD_USERADIALCLIP = 0;
	}

	int GetIndex()
	{
		return ( 1 * m_nD_TEXTURETYPE ) + ( 5 * m_nD_PREMULTIPLY_ALPHA ) +
			   ( 10 * m_nD_USERADIALGRADIENT ) + ( 20 * m_nD_GRADIENT_TWOSTOP ) +
			   ( 40 * m_nD_GRADIENT_COMPLEX ) + ( 80 * m_nD_USEOUTERCORNER ) +
			   ( 160 * m_nD_USEINNERCORNER ) + ( 320 * m_nD_COLORCORRECTION ) +
			   ( 640 * m_nD_USEOPACITYMASK ) + ( 1280 * m_nD_USERADIALCLIP );
	}
};
// The macro's value references the variables SET_DYNAMIC_PIXEL_SHADER_COMBO() declares, the same way
// the generated .inc files do (they are otherwise unused, which this module reports as an error).
#define shaderDynamicTest_panoramafancy_ps30 ( psh_forgot_to_set_dynamic_D_TEXTURETYPE + psh_forgot_to_set_dynamic_D_PREMULTIPLY_ALPHA + psh_forgot_to_set_dynamic_D_USERADIALGRADIENT + psh_forgot_to_set_dynamic_D_GRADIENT_TWOSTOP + psh_forgot_to_set_dynamic_D_GRADIENT_COMPLEX + psh_forgot_to_set_dynamic_D_USEOUTERCORNER + psh_forgot_to_set_dynamic_D_USEINNERCORNER + psh_forgot_to_set_dynamic_D_COLORCORRECTION + psh_forgot_to_set_dynamic_D_USEOPACITYMASK + psh_forgot_to_set_dynamic_D_USERADIALCLIP )
