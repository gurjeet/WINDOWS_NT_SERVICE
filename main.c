#include <windows.h>
#include <tchar.h>
#include <stdio.h>

static TCHAR* ServiceName = TEXT("Beeper");
static SERVICE_STATUS ServiceStatus;
static SERVICE_STATUS_HANDLE hServiceStatus = 0;
static HANDLE evStopService = 0;

/******************************************************************************
 * This functioon is called by the OS to notify a running service of
 * it's intentions (request to start/stop/etc. ) or to query it's status.
 *****************************************************************************/
void WINAPI ServiceControlHandler( DWORD ControlCode )
{
	/* What does the OS want us to do? */
	switch( ControlCode )
	{
		case SERVICE_CONTROL_INTERROGATE:
			/* someone wants to know the controls/requests that we accept; the
			 * last statement in this function will do this for us
			 */
			break;

		case SERVICE_CONTROL_SHUTDOWN:
			/* machine is shutting down */
			/* fall through to STOP the service */

		case SERVICE_CONTROL_STOP:
			/* we are requested to stop the service */

			/* tell the OS that I am going down */
			ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus( hServiceStatus, &ServiceStatus );

			/* tell our service (the other thread) that we should stop now */
			SetEvent( evStopService );
			return;

		case SERVICE_CONTROL_PAUSE:
			/* we are requested to pause the service. That is, stop doing whatever we are
			 * supposed to do, but DO NOT exit from the process
			 */
			ServiceStatus.dwCurrentState = SERVICE_PAUSED;

			break;

		case SERVICE_CONTROL_CONTINUE:
			/* we are in paused state; we should start doing whatever
			 * we are supposed to do
			 */
			ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			break;

		default:
			if( ControlCode >= 128 && ControlCode <= 255 )
			{
				/* we can use one of these control codes to handle
				 * requirements specific to our service
				 */
				break;
			}
			else
			{	/* we are not supposed to interpret any other request; these
				 * codes are reserved by the OS
				 */
				break;
			}
			break;
	}

	/* update our status with the OS */
	SetServiceStatus( hServiceStatus, &ServiceStatus );
}

/* the main() function for our service */
void WINAPI ServiceMain( DWORD /*argc*/, TCHAR* /*argv*/[] )
{

	/* populate our current state in the status object */
	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ServiceStatus.dwWin32ExitCode = NO_ERROR;
	ServiceStatus.dwServiceSpecificExitCode = NO_ERROR;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;

	/* tell the OS that we accept PAUSE/CONTINUE, STOP and SHUTDOWN requests */
	ServiceStatus.dwControlsAccepted |=
		( SERVICE_ACCEPT_PAUSE_CONTINUE
			| SERVICE_ACCEPT_STOP
			| SERVICE_ACCEPT_SHUTDOWN);

	/* register the control handler for the service */
	hServiceStatus = RegisterServiceCtrlHandler( ServiceName, ServiceControlHandler );

	/* registration went fine... go ahead... start the service */
	if( hServiceStatus )
	{
		/* tell the OS that we are starting */
		ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
		SetServiceStatus( hServiceStatus, &ServiceStatus );

		/* let's pull-up our socks... do any initialisation here */
		evStopService = CreateEvent( 0, FALSE, FALSE, 0 );

		/* tell the OS we are in business... our initialisation went fine... */
		ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus( hServiceStatus, &ServiceStatus );

		/* now run... */
		do
		{
			/* beep every 5 seconds */

			/* only if we are running, i.e., not paused */
			if( ServiceStatus.dwCurrentState == SERVICE_RUNNING )
				Beep( 1000, 100 );

			/* until we are asked to stop (by our control-handler) */
		}while( WaitForSingleObject( evStopService, 5000 ) == WAIT_TIMEOUT );

		/* we have been asked to stop; tell the OS that we are ready to do so. */
		ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus( hServiceStatus, &ServiceStatus );

		/* do any cleanups here... */
		CloseHandle( evStopService );
		evStopService = 0;

		/* tell the OS that we have stopped */
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus( hServiceStatus, &ServiceStatus );
	}
}

/* we have just been brought up... let's run the service. */
void RunService()
{
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ ServiceName, ServiceMain },
		{ 0, 0 }
	};

	/* this function returns only after all the services in this process
	 * have stopped. SCM uses this (main) thread as a dispatcher for
	 * threads that are used to run the services
	 */
	StartServiceCtrlDispatcher( ServiceTable );
}

/* we use this function to install the service */
void InstallService()
{
	/* connect to SCM and tell him that we intend to create a service */
	SC_HANDLE ServiceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CREATE_SERVICE );

	if( ServiceControlManager )
	{
		TCHAR Path[ _MAX_PATH + 1 ];

		/* get the name of the .exe that we are running as */
		if( GetModuleFileName( 0, Path, sizeof(Path)/sizeof(Path[0]) ) > 0 )
		{
			/* register this executable as a service */
			SC_HANDLE Service = CreateService(
									ServiceControlManager,
									ServiceName, ServiceName,
									SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
									SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, Path,
									0, 0, 0, 0, 0
								);

			if( Service )
				CloseServiceHandle( Service );
		}

		CloseServiceHandle( ServiceControlManager );
	}
}

/* we use this function to uninstall the service we created earlier */
void UninstallService()
{
	/* connect to SCM in plain CONNECT mode */
	SC_HANDLE ServiceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CONNECT );

	if( ServiceControlManager )
	{
		/* open the service and tell SCM that we wish to QUERY and
		 * DELETE this service
		 */
		SC_HANDLE Service = OpenService(
								ServiceControlManager,
								ServiceName,
								SERVICE_QUERY_STATUS | DELETE
							);

		if( Service )
		{
			SERVICE_STATUS ServiceStatus;/* note that there is another global one */

			/* check service's current status */
			if( QueryServiceStatus( Service, &ServiceStatus ) )
			{
				if( ServiceStatus.dwCurrentState == SERVICE_STOPPED )
				{
					/* delete the service only if it not running
					 * (this is not a pre-requisite to delete a service )
					 */
					DeleteService( Service );
				}
			}

			CloseServiceHandle( Service );
		}

		CloseServiceHandle( ServiceControlManager );
	}
}

/* the 'normal' main() function of the process */
int main( int argc, TCHAR* argv[] )
{
	/* do we have any command line parameters? */
	if( argc > 1 )
	{
		/* are we being asked to install the service */
		if( lstrcmpi( argv[1], TEXT("install") ) == 0 )
		{
			InstallService();
		}
		else
		/* or are we being asked to uninstall it */
		if( argc > 1 && lstrcmpi( argv[1], TEXT("uninstall") ) == 0 )
		{
			UninstallService();
		}
		else
		/* command line parameters are not what we understand; 'stupid dog' */
		{
			printf( "\nUSAGE: <command> [INSTALL|UNINSTALL]\n" );
		}
	}
	else
	/* if not, then just run the service */
	{
		RunService();
	}

	return 0;
}

