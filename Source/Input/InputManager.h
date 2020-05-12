#ifndef INPUT_INPUTMANAGER_H_
#define INPUT_INPUTMANAGER_H_

#include "OSHLE/ultra_os.h"
#include "Utility/Singleton.h"

#include "Math/Vector2.h"

class CInputManager : public CSingleton< CInputManager >
{
	public:
		virtual ~CInputManager() {}

#if defined(DAEDALUS_PSP) || defined(DAEDALUS_VITA)
		virtual u32				GetNumConfigurations() const = 0;
		virtual const char *	GetConfigurationName( u32 configuration_idx ) const = 0;
		virtual const char *	GetConfigurationDescription( u32 configuration_idx ) const = 0;
		virtual void			SetConfiguration( u32 configuration_idx ) = 0;

		virtual u32				GetConfigurationFromName( const char * name ) const = 0;
#endif
		virtual bool Initialise() = 0;
		virtual void Finalise() = 0;

		virtual void GetState( OSContPad pPad[4] ) = 0;

		static bool Init() { return CInputManager::Get()->Initialise();}
		static void Fini() { CInputManager::Get()->Finalise();}
};

#ifdef DAEDALUS_PSP
v2	ApplyDeadzone( const v2 & in, f32 min_deadzone, f32 max_deadzone );
#endif

#endif // INPUT_INPUTMANAGER_H_
