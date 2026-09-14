//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An application framework 
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef IAPPSYSTEM_H
#define IAPPSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"


//-----------------------------------------------------------------------------
// Client systems are singleton objects in the client codebase responsible for
// various tasks
// The order in which the client systems appear in this list are the
// order in which they are initialized and updated. They are shut down in
// reverse order from which they are initialized.
//-----------------------------------------------------------------------------

enum InitReturnVal_t
{
	INIT_FAILED = 0,
	INIT_OK,

	INIT_LAST_VAL,
};


//-----------------------------------------------------------------------------
// Specifies a module + interface name for initialization
//
// SE port: CS:GO declares this (and AppSystemTier_t) in IAppSystem.h.  Source Engine 2013 had
// AppSystemInfo_t only in appframework/IAppSystemGroup.h; the definition below is token-identical
// to that one, so both headers may be included by the same translation unit.
// panorama/source2/panoramauiengine.h derives from IAppSystem and overrides GetDependencies(),
// which only compiles once the type and the virtual exist here.
//-----------------------------------------------------------------------------
struct AppSystemInfo_t
{
	const char *m_pModuleName;
	const char *m_pInterfaceName;
};


enum AppSystemTier_t
{
	APP_SYSTEM_TIER0 = 0,
	APP_SYSTEM_TIER1,
	APP_SYSTEM_TIER2,
	APP_SYSTEM_TIER3,

	APP_SYSTEM_TIER_OTHER,
};


abstract_class IAppSystem
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory ) = 0;
	virtual void Disconnect() = 0;

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName ) = 0;

	// Init, shutdown
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;

	// SE port (CS:GO addition): additive, defaulted virtuals.  Existing Source Engine
	// implementations keep compiling and panorama's source2 UI engine can override
	// GetDependencies() with OVERRIDE.
	virtual const AppSystemInfo_t *GetDependencies() { return NULL; }
	virtual AppSystemTier_t GetTier() { return APP_SYSTEM_TIER_OTHER; }
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) {}
	virtual bool IsSingleton() { return true; }
};


//-----------------------------------------------------------------------------
// Helper empty implementation of an IAppSystem
//-----------------------------------------------------------------------------
template< class IInterface > 
class CBaseAppSystem : public IInterface
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory ) { return true; }
	virtual void Disconnect() {}

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName ) { return NULL; }

	// Init, shutdown
	virtual InitReturnVal_t Init() { return INIT_OK; }
	virtual void Shutdown() {}

	// SE port (CS:GO addition): see the note on IAppSystem above.
	virtual const AppSystemInfo_t *GetDependencies() { return NULL; }
	virtual AppSystemTier_t GetTier() { return APP_SYSTEM_TIER_OTHER; }
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) {}
	virtual bool IsSingleton() { return true; }
};


//-----------------------------------------------------------------------------
// Helper implementation of an IAppSystem for tier0
//-----------------------------------------------------------------------------
template< class IInterface > 
class CTier0AppSystem : public CBaseAppSystem< IInterface >
{
public:
	CTier0AppSystem( bool bIsPrimaryAppSystem = true )
	{
		m_bIsPrimaryAppSystem = bIsPrimaryAppSystem;
	}

protected:
	// NOTE: a single DLL may have multiple AppSystems it's trying to
	// expose. If this is true, you must return true from only
	// one of those AppSystems; not doing so will cause all static
	// libraries connected to it to connect/disconnect multiple times

	// NOTE: We don't do this as a virtual function to avoid
	// having to up the version on all interfaces
	bool IsPrimaryAppSystem() { return m_bIsPrimaryAppSystem; }

private:
	bool m_bIsPrimaryAppSystem;
};


//-----------------------------------------------------------------------------
// This is the version of IAppSystem shipped 10/15/04
// NOTE: Never change this!!!
//-----------------------------------------------------------------------------
abstract_class IAppSystemV0
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory ) = 0;
	virtual void Disconnect() = 0;

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName ) = 0;

	// Init, shutdown
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;
};

#endif // IAPPSYSTEM_H

