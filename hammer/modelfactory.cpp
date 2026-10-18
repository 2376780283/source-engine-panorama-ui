//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "mapstudiomodel.h"
#include "modelfactory.h"


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eModelType - 
//			pszModelData - 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CModelFactory::CreateModel(ModelType_t eModelType, const char *pszModelData)
{
	switch (eModelType)
	{
		case ModelTypeStudio:
		{
			int nLen = strlen(pszModelData);
			if ((nLen > 4) && (!stricmp(&pszModelData[nLen - 4], ".mdl")))
			{
				CMapStudioModel *pModel = new CMapStudioModel(pszModelData);
				return(pModel);
			}
			break;
		}

		default:
		{
			break;
		}
	}

	return(NULL);
}
