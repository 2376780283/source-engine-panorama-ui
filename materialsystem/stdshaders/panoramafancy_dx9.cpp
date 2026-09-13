//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//

// SE port: ported from CS:GO's materialsystem/stdshaders/panoramafancy_cshader.cpp.  Same deltas as
// panorama_dx9.cpp: CBaseShader::BindTexture() here takes no TextureBindFlags_t argument, and the
// combo index classes come from the hand-written fxctmp9/panoramafancy_{vs30,ps30}.inc.

#include "BaseVSShader.h"
#include "shaderlib/cshader.h"

#include "panoramafancy_vs30.inc"
#include "panoramafancy_ps30.inc"

#include "panorama/s1wrapperRenderAttributes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( panoramafancy, panoramafancy_dx9 )
BEGIN_VS_SHADER( panoramafancy_dx9, "Help for panorama" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BLENDSTATE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( RENDERATTR, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#ifdef PLATFORM_64BITS
		SHADER_PARAM( RENDERATTR_HIGH, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#endif
	END_SHADER_PARAMS

	SHADER_INIT
	{
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_ALWAYS, 0 );
			pShaderShadow->EnableAlphaTest( false );

			pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableDepthTest( false );

			pShaderShadow->EnableSRGBWrite( true );

			// SE port: panorama hands the material system vertices that are already in clip space, and that
			// transform flips Y (D3D clip space has +Y up, panorama's coordinates grow downwards), which
			// reverses the triangle winding.  The material system's default state is culling enabled with
			// D3DCULL_CCW, so every panorama quad was being discarded by the rasteriser: the draw was
			// submitted with the right geometry and colour and simply never produced a pixel.  CS:GO's own
			// D3D path draws panorama with culling off, so match that here.
			pShaderShadow->EnableCulling( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

			pShaderShadow->EnableAlphaToCoverage( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->EnableBlendingSeparateAlpha( true );
			pShaderShadow->EnableColorWrites( true );
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->BlendOp( SHADER_BLEND_OP_ADD );

			int blendState = params[ BLENDSTATE ]->GetIntValue();

			switch ( blendState )
			{
			case BLENDSTATE_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			// Premultiplied Alpha Blend
			case BLENDSTATE_PREMULT_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			// Alpha Only Blend
			case BLENDSTATE_ONLY_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_MULTIPLY:
				pShaderShadow->BlendFunc( SHADER_BLEND_DST_COLOR, SHADER_BLEND_ZERO );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_SCREEN:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE_MINUS_DST_COLOR, SHADER_BLEND_ONE );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_ADDITIVE:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_ADDITIVESRGB:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_OPAQUE:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ZERO );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			default:
				DevWarning( "Invalid blend mode (%d) using pan_dx 0\n", blendState );
				pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;
			}

			// Set stream format (the s1wrapper mesh builder writes position + 5 texcoords)
			unsigned int flags = VERTEX_POSITION;
			int nTexCoordCount = 5;
			int userDataSize = 0;
			static int s_TexCoordSize[] = { 4, 4, 4, 4, 4, 4, 4, 4 };
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, s_TexCoordSize, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( panoramafancy_vs30 );
			SET_STATIC_VERTEX_SHADER( panoramafancy_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( panoramafancy_ps30 );
			SET_STATIC_PIXEL_SHADER( panoramafancy_ps30 );
		}

		DYNAMIC_STATE
		{
			// SE port: not every panorama draw path goes through the s1wrapper's UpdateMaterial(), which is
			// what installs $renderattr on the material, so this shader can be reached with that var unset.
			// CS:GO's shader assumes it is always present; dereferencing it NULL crashed the game as soon as
			// the CS:GO main menu started painting (panoramafancy_dx9.cpp:143, access at address 0).
			if ( !params[ RENDERATTR ] )
			{
				static bool s_bWarnedMissingRenderAttr = false;
				if ( !s_bWarnedMissingRenderAttr )
				{
					s_bWarnedMissingRenderAttr = true;
					Warning( "panorama: panoramafancy draw without $renderattr - skipping the draw\n" );
				}
				return;
			}

#ifdef PLATFORM_64BITS
			CRenderAttributes* pAttr = (CRenderAttributes*)( ( uint64( params[ RENDERATTR_HIGH ]->GetIntValue() ) << 32 ) | ( uint64( params[ RENDERATTR ]->GetIntValue() ) & 0xffffffff ) );
#else
			CRenderAttributes* pAttr = (CRenderAttributes*)params[ RENDERATTR ]->GetIntValue();
#endif

			// SE port: the material var can also be present but zero.  panorama materials are shared, and the
			// s1wrapper only writes a live CRenderAttributes pointer for the draw path that ran through
			// UpdateMaterial(); any other path leaves the previous (or zero) value behind, and dereferencing
			// it crashed the game at line 178 with access at address 0.
			if ( !pAttr )
			{
				static bool s_bWarnedNullRenderAttr = false;
				if ( !s_bWarnedNullRenderAttr )
				{
					s_bWarnedNullRenderAttr = true;
					Warning( "panorama: panoramafancy draw with a null $renderattr pointer - skipping the draw\n" );
				}
				return;
			}

			// Vertex Shader - no longer require VS consts

			// Pixel Shader
			ITexture *pTexture = NULL;

			int texType = pAttr->GetValue( ATTR_D_TEXTURETYPE );

			// SE port (bring-up aid): what does the fancy shader actually get for a solid colour fill?
			{
				static int s_nSEFancyShaderLogged = 0;
				if ( s_nSEFancyShaderLogged < 16 )
				{
					s_nSEFancyShaderLogged++;

					ITexture *pProbeTex0 = NULL;
					pAttr->GetValue( &pProbeTex0, ATTR_Texture0 );

				}
			}

			// SE port: panorama's render attributes can still reference a texture that has already been
			// destroyed in this port - they are not pooled across frames here, and an image whose load failed
			// is torn down.  Binding one of those jumped through a freed vtable inside
			// CShaderSystem::BindTexture (access at address 0), which killed the game as soon as the CS:GO
			// menu painted.  The textures are simply not bound below, so the sampler reads the D3D default;
			// the colours come from the pixel shader's own combo state.
			if ( texType )
			{
				pAttr->GetValue( &pTexture, ATTR_Texture0 );
				if ( !pTexture )
				{
					// SE port: the layout asked for a texture that this port could not load (the CS:GO menu's
					// images are not unpacked into the mod yet), so fall back to the untextured path instead of
					// dereferencing NULL - that crashed here (access at address 0).
					static bool s_bWarnedMissingTexture = false;
					if ( !s_bWarnedMissingTexture )
					{
						s_bWarnedMissingTexture = true;
						Warning( "panorama: fancy draw with a missing texture - drawing it untextured\n" );
					}
					// NOTE: texType is deliberately NOT cleared here.  It is one of the inputs to the pixel
					// shader combo index (see PanDxSetShadersFancy), so overriding it made the combo disagree
					// with the shader's own D_TEXTURETYPE and every panel rendered white.
				}
				else
				{
					// SE port: bind the texture.  D3D9's samplers read white while nothing is bound, and not
					// binding is exactly what turned the whole CS:GO menu into a white screen once the image
					// resources were unpacked into the mod.
					//
					// The pointer still has to be checked: an image this port cannot decode (the .vsvg icons,
					// see the note further down) leaves an attribute that no longer points at a live texture,
					// and binding one of those crashed inside CShaderSystem::BindTexture with access at a
					// value that was a float bit pattern (0x8B000000).  Only bind what can be a real object.
					uintp pTexBits = (uintp)pTexture;
					if ( pTexBits >= 0x10000 && pTexBits < 0x7FFF0000 && ( pTexBits & 3 ) == 0 )
					{
						BindTexture( SHADER_SAMPLER0, pTexture );
					}

					if ( texType == 4 )
					{
						// SE port: this used to read pTexture->GetFlags() to decide whether a type-4 texture is
						// really YCoCg.  pTexture comes out of the render attributes and can already be a
						// destroyed texture object in this port (the attributes are not pooled across frames
						// here), so the call went through a freed vtable and crashed with access at address
						// 0x6D as soon as the menu painted with real images installed.  Nothing is bound below
						// (see the note above), and texType is also an input to the pixel shader combo index
						// (PanDxSetShadersFancy), so it is left exactly as the layout asked for it.
					}
				}
			}

			if ( pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX ) )
			{
				// SE port: see the note above - the extra texture slots are not bound here.
			}

			Vector4D vTopCornerRad, vBtmCornerRad;
			pAttr->GetValue( &vTopCornerRad, ATTR_TopCornerRad );
			pAttr->GetValue( &vBtmCornerRad, ATTR_BtmCornerRad );

			vTopCornerRad += Vector4D( 0.5, 0.5, 0.5, 0.5 );
			vBtmCornerRad += Vector4D( 0.5, 0.5, 0.5, 0.5 );

			Vector4D vWd = pAttr->GetValue( ATTR_BorderWd ) + Vector4D( 0.5, 0.5, 0.5, 0.5 );

			Vector4D vGradientRadialOffset, vOpacityMaskOpacity, vH, vS, vB, vC;
			pAttr->GetValue( &vGradientRadialOffset, ATTR_Gradientradialoffset );
			pAttr->GetValue( &vOpacityMaskOpacity, ATTR_OpacityMaskOpacity );
			pAttr->GetValue( &vH, ATTR_HueShift );
			pAttr->GetValue( &vS, ATTR_Saturation );
			pAttr->GetValue( &vB, ATTR_Brightness );
			pAttr->GetValue( &vC, ATTR_Contrast );

			Vector4D vRadialClipInfo;
			if ( pAttr->GetValue( ATTR_D_USERADIALCLIP ) )
			{
				Vector4D vRadialClipTmp;
				pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipCenterX );	// currently not used
				vRadialClipInfo.x = vRadialClipTmp.x;
				pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipCenterY ); // currently not used
				vRadialClipInfo.y = vRadialClipTmp.y;
				pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipStartAngle );
				vRadialClipInfo.z = vRadialClipTmp.z;
				pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipSectorAngle );
				vRadialClipInfo.w = vRadialClipTmp.w;
			}

			// packing PS consts
			vGradientRadialOffset.z = vOpacityMaskOpacity.x;
			vGradientRadialOffset.w = 0.0f; // unused
			vH.y = vS.x;
			vH.z = vB.x;
			vH.w = vC.x;

			pShaderAPI->SetPixelShaderConstant( 0, vTopCornerRad.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 1, vBtmCornerRad.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 2, vWd.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 3, pAttr->GetValue( ATTR_Bordercolor ).Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 4, vGradientRadialOffset.Base(), 1 );  // w currently not used
			pShaderAPI->SetPixelShaderConstant( 5, vH.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 6, vRadialClipInfo.Base(), 1 ); // xy currently not used but reserved for radial clip center

			// Combos

			DECLARE_DYNAMIC_VERTEX_SHADER( panoramafancy_vs30 );
			SET_DYNAMIC_VERTEX_SHADER( panoramafancy_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( panoramafancy_ps30 );

			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEXTURETYPE, texType );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_PREMULTIPLY_ALPHA, pAttr->GetValue( ATTR_D_PREMULTIPLY_ALPHA ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USERADIALGRADIENT, pAttr->GetValue( ATTR_D_USERADIALGRADIENT ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_GRADIENT_TWOSTOP, pAttr->GetValue( ATTR_D_GRADIENT_TWOSTOP ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_GRADIENT_COMPLEX, pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEOUTERCORNER, pAttr->GetValue( ATTR_D_USEOUTERCORNER ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEINNERCORNER, pAttr->GetValue( ATTR_D_USEINNERCORNER ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_COLORCORRECTION, pAttr->GetValue( ATTR_D_COLORCORRECTION ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEOPACITYMASK, pAttr->GetValue( ATTR_D_USEOPACITYMASK ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USERADIALCLIP, pAttr->GetValue( ATTR_D_USERADIALCLIP ) );

			SET_DYNAMIC_PIXEL_SHADER( panoramafancy_ps30 );
		}
		Draw();
	}
END_SHADER
