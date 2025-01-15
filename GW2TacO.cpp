#include "OverlayApplication.h"
#include "ProFont.h"
#include <dwmapi.h>
#include "gw2tactical.h"
#include "TrailLogger.h"
#include "MapTimer.h"
#include "MouseHighlight.h"
#include "MumbleLink.h"
#include <process.h>
#include <windowsx.h>
#include "OverlayConfig.h"
#include "GW2TacOMenu.h"
#include "OverlayWindow.h"
#include "resource.h"
#include "LocationalTimer.h"
#include "BuildCount.h"
#include "RangeDisplay.h"
#include "TacticalCompass.h"
#include "HPGrid.h"
#include "GW2API.h"
#include "SpecialGUIItems.h"
#include "Language.h"
#include <locale>
#include <codecvt>
#include "InputHooks.h"
#include <ShellScalingAPI.h>
#include "WvW.h"
#include <TlHelp32.h>
#include <imm.h>
#include "MarkerPack.h"
#include "MarkerEditor.h"

#define MINIZ_HEADER_FILE_ONLY
#include "Bedrock/UtilLib/miniz.c"

#pragma comment(lib,"Dwmapi.lib")
#pragma comment(lib,"Imm32.lib")

CLoggerOutput_RingBuffer* ringbufferLog = nullptr;

CWBApplication* App = NULL;
HWND gameWindow;
HWND gw2WindowFromPid = nullptr;
TBOOL isTacOUptoDate = true;
int newTacOVersion = RELEASECOUNT;
int gw2WindowCount = 0;

TBOOL InitGUI( CWBApplication* App )
{
  CreateUniFontOutlined( App, "UniFontOutlined" ); // default font
  CreateProFontOutlined( App, "ProFontOutlined" );
  CreateUniFont( App, "UniFont" );
  CreateProFont( App, "ProFont" );

  if ( !App->LoadSkinFromFile( _T( "UI.wbs" ), Localization::GetUsedGlyphs() ) )
  {
    MessageBox( NULL, _T( "TacO can't find the UI.wbs ui skin file!\nPlease make sure you extracted all the files from the archive to a separate folder!" ), _T( "Missing File!" ), MB_ICONERROR );
    return false;
  }
  if ( !App->LoadXMLLayoutFromFile( _T( "UI.xml" ) ) )
  {
    MessageBox( NULL, _T( "TacO can't find the UI.xml ui layout file!\nPlease make sure you extracted all the files from the archive to a separate folder!" ), _T( "Missing File!" ), MB_ICONERROR );
    return false;
  }
  if ( !App->LoadCSSFromFile( UIFileNames[ Config::GetValue( "InterfaceSize" ) ] ) )
  {
    MessageBox( NULL, _T( "TacO can't find a required UI css style file!\nPlease make sure you extracted all the files from the archive to a separate folder!" ), _T( "Missing File!" ), MB_ICONERROR );
    return false;
  }
  App->RegisterUIFactoryCallback( "GW2TacticalDisplay", GW2TacticalDisplay::Factory );
  App->RegisterUIFactoryCallback( "GW2TrailDisplay", GW2TrailDisplay::Factory );
  App->RegisterUIFactoryCallback( "GW2MapTimer", GW2MapTimer::Factory );
  App->RegisterUIFactoryCallback( "MouseHighlight", GW2MouseHighlight::Factory );
  App->RegisterUIFactoryCallback( "GW2TacO", GW2TacO::Factory );
  App->RegisterUIFactoryCallback( "OverlayWindow", OverlayWindow::Factory );
  App->RegisterUIFactoryCallback( "TimerDisplay", TimerDisplay::Factory );
  App->RegisterUIFactoryCallback( "GW2TacticalCompass", GW2TacticalCompass::Factory );
  App->RegisterUIFactoryCallback( "GW2RangeDisplay", GW2RangeDisplay::Factory );
  App->RegisterUIFactoryCallback( "HPGrid", GW2HPGrid::Factory );
  App->RegisterUIFactoryCallback( "clickthroughbutton", ClickThroughButton::Factory );

  App->GenerateGUI( App->GetRoot(), _T( "gw2pois" ) );

  App->ReApplyStyle();

  return true;
}

void OpenOverlayWindows( CWBApplication* App )
{
  auto root = App->GetRoot();
  GW2TacO* taco = (GW2TacO*)root->FindChildByID( "tacoroot", "GW2TacO" );
  if ( !taco )
    return;

  if ( Config::HasWindowData( "MapTimer" ) && Config::IsWindowOpen( "MapTimer" ) )
    taco->OpenWindow( "MapTimer" );

  if ( Config::HasWindowData( "TS3Control" ) && Config::IsWindowOpen( "TS3Control" ) )
    taco->OpenWindow( "TS3Control" );

  if ( Config::HasWindowData( "MarkerEditor" ) && Config::IsWindowOpen( "MarkerEditor" ) )
    taco->OpenWindow( "MarkerEditor" );

  if ( Config::HasWindowData( "Notepad" ) && Config::IsWindowOpen( "Notepad" ) )
    taco->OpenWindow( "Notepad" );

  if ( Config::HasWindowData( "RaidProgress" ) && Config::IsWindowOpen( "RaidProgress" ) )
    taco->OpenWindow( "RaidProgress" );

  if ( Config::HasWindowData( "DungeonProgress" ) && Config::IsWindowOpen( "DungeonProgress" ) )
    taco->OpenWindow( "DungeonProgress" );

  if ( Config::HasWindowData( "TPTracker" ) && Config::IsWindowOpen( "TPTracker" ) )
    taco->OpenWindow( "TPTracker" );
}

LONG WINAPI CrashOverride( struct _EXCEPTION_POINTERS* excpInfo )
{
  if ( IsDebuggerPresent() ) return EXCEPTION_CONTINUE_SEARCH;

    LONG res = baseCrashTracker( excpInfo );// FullDumpCrashTracker( excpInfo );// baseCrashTracker( excpInfo );
    return res;
}

DWORD GetProcessIntegrityLevel( HANDLE hProcess )
{
  HANDLE hToken;

  DWORD dwLengthNeeded;
  DWORD dwError = ERROR_SUCCESS;

  PTOKEN_MANDATORY_LABEL pTIL = NULL;
  DWORD dwIntegrityLevel = 0;

  if ( OpenProcessToken( hProcess, TOKEN_QUERY, &hToken ) )
  {
    if ( !GetTokenInformation( hToken, TokenIntegrityLevel, NULL, 0, &dwLengthNeeded ) )
    {
      dwError = GetLastError();
      if ( dwError == ERROR_INSUFFICIENT_BUFFER )
      {
        pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc( 0, dwLengthNeeded );
        if ( pTIL != NULL )
        {
          if ( GetTokenInformation( hToken, TokenIntegrityLevel, pTIL, dwLengthNeeded, &dwLengthNeeded ) )
            dwIntegrityLevel = *GetSidSubAuthority( pTIL->Label.Sid, (DWORD)(UCHAR)( *GetSidSubAuthorityCount( pTIL->Label.Sid ) - 1 ) );
          LocalFree( pTIL );
        }
      }
    }
    else
      return -1;
    CloseHandle( hToken );
  }
  else
    return -1;
  return dwIntegrityLevel;
}

bool IsProcessRunning( DWORD pid )
{
  bool procRunning = false;

  HANDLE hProcessSnap;
  PROCESSENTRY32 pe32;
  hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

  if ( hProcessSnap == INVALID_HANDLE_VALUE )
    return false;

  pe32.dwSize = sizeof( PROCESSENTRY32 );
  if ( Process32First( hProcessSnap, &pe32 ) )
  {
    if ( pe32.th32ProcessID == pid )
    {
      CloseHandle( hProcessSnap );
      return true;
    }

    while ( Process32Next( hProcessSnap, &pe32 ) )
    {
      if ( pe32.th32ProcessID == pid )
      {
        CloseHandle( hProcessSnap );
        return true;
      }
    }
    CloseHandle( hProcessSnap );
  }

  return procRunning;
}

void GetFileName( CHAR pfname[ MAX_PATH ] )
{
  DWORD dwOwnPID = GetProcessId( GetCurrentProcess() );

  HANDLE hSnapShot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
  PROCESSENTRY32* processInfo = new PROCESSENTRY32;
  processInfo->dwSize = sizeof( PROCESSENTRY32 );
  while ( Process32Next( hSnapShot, processInfo ) != FALSE )
  {
    if ( processInfo->th32ProcessID == dwOwnPID )
    {
      memcpy( pfname, processInfo->szExeFile, MAX_PATH );
      break;
    }
  }
  CloseHandle( hSnapShot );
  delete processInfo;
}

BOOL IsTacORunning()
{
  CHAR pfname[ MAX_PATH ];
  memset( pfname, 0, MAX_PATH );
  GetFileName( pfname );

  BOOL bRunning = FALSE;

  DWORD dwOwnPID = GetProcessId( GetCurrentProcess() );

  HANDLE hSnapShot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
  PROCESSENTRY32* processInfo = new PROCESSENTRY32;
  processInfo->dwSize = sizeof( PROCESSENTRY32 );
  int index = 0;
  while ( Process32Next( hSnapShot, processInfo ) != FALSE )
  {
    if ( !strcmp( processInfo->szExeFile, pfname ) )
    {
      if ( processInfo->th32ProcessID != dwOwnPID )
      {
        bRunning = TRUE;
        break;
      }
    }
  }
  CloseHandle( hSnapShot );
  delete processInfo;
  return bRunning;
}

bool SetupTacoProtocolHandling()
{
  TCHAR szFileName[ MAX_PATH + 1 ];
  GetModuleFileName( NULL, szFileName, MAX_PATH + 1 );

  HKEY key;

  if ( RegCreateKeyEx( HKEY_CLASSES_ROOT, "gw2taco", 0, nullptr, REG_OPTION_NON_VOLATILE, 0, nullptr, &key, nullptr ) != ERROR_SUCCESS )
  {
    if ( RegOpenKey( HKEY_CLASSES_ROOT, "gw2taco", &key ) != ERROR_SUCCESS )
      return false;
  }
  else
  {
    RegCloseKey( key );
    if ( RegOpenKey( HKEY_CLASSES_ROOT, "gw2taco", &key ) != ERROR_SUCCESS )
      return false;
  }

  const char* urldesc = "URL:gw2taco protocol";

  if ( RegSetKeyValue( key, nullptr, nullptr, REG_SZ, urldesc, strlen( urldesc ) ) != ERROR_SUCCESS )
    return false;

  if ( RegSetKeyValue( key, nullptr, "URL Protocol", REG_SZ, nullptr, 0 ) )
    return false;

  if ( RegSetKeyValue( key, "DefaultIcon", nullptr, REG_SZ, szFileName, strlen( szFileName ) ) != ERROR_SUCCESS )
    return false;

  CString openMask = CString( "\"" ) + CString( szFileName ) + CString( "\" -fromurl %1" );

  if ( RegSetKeyValue( key, "shell\\open\\command", nullptr, REG_SZ, openMask.GetPointer(), openMask.Length() ) != ERROR_SUCCESS )
    return false;

  RegCloseKey( key );
  return true;
}

void FetchMarkerPackOnline( CString& ourl )
{
  TS32 pos = ourl.Find( "gw2taco://markerpack/" );
  if ( pos < 0 )
  {
    LOG_ERR( "[GW2TacO] Trying to access malformed package url %s", ourl.GetPointer() );
    return;
  }

  LOG_NFO( "[GW2TacO] Trying to fetch marker pack %s", ourl.Substring( pos ).GetPointer() );

  CString url = ourl.Substring( pos + 21 );
  TU8* urlPtr = new TU8[ url.Length() + 1 ];
  memset( urlPtr, 0, url.Length() + 1 );
  memcpy( urlPtr, url.GetPointer(), url.Length() );

  DWORD downloadThreadID = 0;

  auto downloadThread = CreateThread( NULL, 0, []( LPVOID data )
                                      {
                                        CString url( (TS8*)data );
                                        delete[] data;

                                        CStreamWriterMemory mem;
                                        if ( !DownloadFile( url, mem ) )
                                        {
                                          LOG_ERR( "[GW2TacO] Failed to download package %s", url.GetPointer() );
                                          return (DWORD)0;
                                        }

                                        mz_zip_archive zip;
                                        memset( &zip, 0, sizeof( zip ) );
                                        if ( !mz_zip_reader_init_mem( &zip, mem.GetData(), mem.GetLength(), 0 ) )
                                        {
                                          LOG_ERR( "[GW2TacO] Package %s doesn't seem to be a well formed zip file", url.GetPointer() );
                                          return (DWORD)0;
                                        }

                                        mz_zip_reader_end( &zip );

                                        TS32 cnt = 0;
                                        for ( TU32 x = 0; x < url.Length(); x++ )
                                          if ( url[ x ] == '\\' || url[ x ] == '/' )
                                            cnt = x;

                                        CString fileName = url.Substring( cnt + 1 );
                                        if ( !fileName.Length() )
                                        {
                                          LOG_ERR( "[GW2TacO] Package %s has a malformed name", url.GetPointer() );
                                          return (DWORD)0;
                                        }

                                        if ( fileName.Find( ".zip" ) == fileName.Length() - 4 )
                                          fileName = fileName.Substring( 0, fileName.Length() - 4 );

                                        if ( fileName.Find( ".taco" ) == fileName.Length() - 5 )
                                          fileName = fileName.Substring( 0, fileName.Length() - 5 );

                                        for ( TU32 x = 0; x < fileName.Length(); x++ )
                                          if ( !isalnum( fileName[ x ] ) )
                                            fileName[ x ] = '_';

                                        fileName = CString( "POIs/" ) + fileName + ".taco";

                                        CStreamWriterFile out;
                                        if ( !out.Open( fileName.GetPointer() ) )
                                        {
                                          LOG_ERR( "[GW2TacO] Failed to open file for writing: %s", fileName.GetPointer() );
                                          return (DWORD)0;
                                        }

                                        if ( !out.Write( mem.GetData(), mem.GetLength() ) )
                                        {
                                          LOG_ERR( "[GW2TacO] Failed to write out data to file: %s", fileName.GetPointer() );
                                          remove( fileName.GetPointer() );
                                          return (DWORD)0;
                                        }

                                        markerPackQueue.Add( fileName );

                                        return (DWORD)0;
                                      }, urlPtr, 0, &downloadThreadID );
}

BOOL __stdcall GW2WindowFromPIDFunction( HWND hWnd, LPARAM a2 )
{
  DWORD dwProcessId;
  CHAR ClassName[ 400 ];

  memset( &ClassName, 0, 400 );
  GetClassNameA( hWnd, ClassName, 199 );
  if ( !strcmp( ClassName, "ArenaNet_Dx_Window_Class" ) || !strcmp( ClassName, "ArenaNet_Gr_Window_Class" ) )
  {
    dwProcessId = 0;
    GetWindowThreadProcessId( hWnd, &dwProcessId );
    if ( a2 == dwProcessId )
      gw2WindowFromPid = hWnd;
  }
  return 1;
}

void InitLogging()
{
  Logger.AddOutput( new CLoggerOutput_File( _T( "GW2TacO.log" ) ) );
  ringbufferLog = new CLoggerOutput_RingBuffer();
  Logger.AddOutput( ringbufferLog );
  Logger.SetVerbosity( LOG_DEBUG );

  Logger.Log( LOG_INFO, false, false, "" );
  Logger.Log( LOG_INFO, false, false, "----------------------------------------------" );

  extern CString TacOBuild;
  LOG_NFO( "[GW2TacO] build ID: %s", ( CString( "GW2 TacO " ) + TacOBuild ).GetPointer() );
}

INT ProcessCommandLine()
{
  CString cmdLine( GetCommandLineA() );
  LOG_NFO( "[GW2TacO] CommandLine: %s", cmdLine.GetPointer() );

  if ( cmdLine.Find( "-fromurl" ) >= 0 )
  {
    TCHAR szFileName[ MAX_PATH + 1 ];
    GetModuleFileName( NULL, szFileName, MAX_PATH + 1 );
    CString s( szFileName );
    for ( TS32 x = s.Length() - 1; x >= 0; x-- )
      if ( s[ x ] == '\\' || s[ x ] == '/' )
      {
        s[ x ] = 0;
        break;
      }
    SetCurrentDirectory( s.GetPointer() );

    auto TacoWindow = FindWindow( "CoRE2", "Guild Wars 2 Tactical Overlay" );
    LOG_NFO( "[GW2TacO] TacO window id: %d", TacoWindow );
    if ( TacoWindow )
    {
      COPYDATASTRUCT MyCDS;
      MyCDS.dwData = 0;
      MyCDS.cbData = cmdLine.Length();
      MyCDS.lpData = cmdLine.GetPointer();

      SendMessage( TacoWindow,
                   WM_COPYDATA,
                   (WPARAM)( HWND )nullptr,
                   (LPARAM)(LPVOID)&MyCDS );

      LOG_NFO( "[GW2TacO] WM_COPYDATA sent. Result code: %d", GetLastError() );
      return 0;
    }

    FetchMarkerPackOnline( cmdLine );
  }

  if ( cmdLine.Find( "-forcenewinstance" ) < 0 )
  {
    if ( IsTacORunning() )
      return 0;
  }

  auto mumblePos = cmdLine.Find( "-mumble" );
  if ( mumblePos >= 0 )
  {
    auto sub = cmdLine.Substring( mumblePos );
    auto cmds = sub.ExplodeByWhiteSpace();
    if ( cmds.NumItems() > 1 )
      mumbleLink.mumblePath = cmds[ 1 ];
  }

  if ( cmdLine.Find( "-forcedpiaware" ) >= 0 || ( Config::HasValue( "ForceDPIAware" ) && Config::GetValue( "ForceDPIAware" ) ) )
  {
    bool dpiSet = false;

    typedef HRESULT( WINAPI* SetProcessDpiAwareness )( _In_ PROCESS_DPI_AWARENESS value );
    typedef BOOL( *SetProcessDPIAwareFunc )( );

    HMODULE hShCore = LoadLibrary( _T( "Shcore.dll" ) );
    if ( hShCore )
    {
      SetProcessDpiAwareness setDPIAwareness = (SetProcessDpiAwareness)GetProcAddress( hShCore, "SetProcessDpiAwareness" );
      if ( setDPIAwareness )
      {
        setDPIAwareness( PROCESS_PER_MONITOR_DPI_AWARE );
        dpiSet = true;
        LOG_NFO( "[GW2TacO] DPI Awareness set through SetProcessDpiAwareness" );
      }
      FreeLibrary( hShCore );
    }

    if ( !dpiSet )
    {
      HMODULE hUser32 = LoadLibrary( _T( "user32.dll" ) );
      SetProcessDPIAwareFunc setDPIAware = (SetProcessDPIAwareFunc)GetProcAddress( hUser32, "SetProcessDPIAware" );
      if ( setDPIAware )
      {
        setDPIAware();
        LOG_NFO( "[GW2TacO] DPI Awareness set through SetProcessDpiAware" );
        dpiSet = true;
      }
      FreeLibrary( hUser32 );
    }

    if ( !dpiSet )
      LOG_ERR( "[GW2TacO] DPI Awareness NOT set" );
  }

  return 1;
}

bool InitOverlayApp( HINSTANCE hInstance )
{
  if ( !SetupTacoProtocolHandling() )
    LOG_ERR( "[GW2TacO] Failed to register gw2taco:// protocol with windows." );

  bool hasDComp = 0;
  HMODULE dComp = LoadLibraryA( "dcomp.dll" );
  if ( dComp )
  {
    hasDComp = 1;
    FreeLibrary( dComp );
  }

  App = new COverlayApp();

  int width = 1;
  int height = 1;

  CCoreWindowParameters p = CCoreWindowParameters( GetModuleHandle( NULL ), false, width, height, _T( "Guild Wars 2 Tactical Overlay" ), LoadIcon( hInstance, MAKEINTRESOURCE( IDI_ICON2 ) ) );

  int toolex = Config::GetValue( "TacOOnTaskBar" ) ? 0 : WS_EX_TOOLWINDOW;

  p.OverrideWindowStyle = WS_POPUP;
  p.OverrideWindowStyleEx = WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TRANSPARENT | toolex;// | WS_EX_TOPMOST;// | WS_EX_TOOLWINDOW;
  if ( dComp )
    p.OverrideWindowStyleEx = WS_EX_TOPMOST | WS_EX_TRANSPARENT | toolex | WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP;

  if ( !App->Initialize( p ) )
  {
    MessageBox( NULL, "Failed to initialize GW2 TacO!\nIf you want this error fixed,\nplease send the generated GW2TacO.log and a dxdiag log to boyc@scene.hu", "Fail.", MB_ICONERROR );
    SAFEDELETE( App );
    return false;
  }

  if ( !ChangeWindowMessageFilterEx( (HWND)App->GetHandle(), WM_COPYDATA, MSGFLT_ALLOW, NULL ) )
    LOG_ERR( "[GW2TacO] Failed to change message filters for WM_COPYDATA - gw2taco:// protocol messages will NOT be processed!" );

  App->SetScreenshotName( _T( "GW2TacO" ) );
  App->SetClearColor( CColor( 0, 0, 0, 0 ) );
  App->SetVSync( Config::GetValue( "Vsync" ) );

  if ( !InitGUI( App ) )
  {
    LOG_ERR( "[GW2TacO] Missing file during init, exiting!" );
    return false;
  }

  SetLayeredWindowAttributes( (HWND)App->GetHandle(), 0, 255, LWA_ALPHA );

  return true;
}

void CheckForNewTacOBuild()
{
  if ( Config::GetValue( _T( "CheckForUpdates" ) ) )
  {
    DWORD UpdateCheckThreadID = 0;
    auto hookThread = CreateThread( NULL, 0,
                                    []( LPVOID data )
                                    {
                                      CString s = FetchHTTP( L"www.gw2taco.com", L"/2000/01/buildid.html" );
                                      TS32 idpos = s.Find( "[buildid:" );
                                      if ( idpos >= 0 )
                                      {
                                        CString sub = s.Substring( idpos );
                                        TS32 release = 0;
                                        TS32 build = 0;
                                        if ( sub.Scan( "[buildid:%d.%dr]", &release, &build ) == 2 )
                                        {
                                          extern TS32 TacORelease;
                                          extern TS32 TacOBuildCount;
                                          if ( release > TacORelease || build > TacOBuildCount )
                                          {
                                            newTacOVersion = release;
                                            isTacOUptoDate = false;
                                          }
                                        }
                                      }

                                      return (DWORD)0;
                                    },
                                    0, 0, &UpdateCheckThreadID );
  }
}

void ValidateGW2ProcessIntegrity( DWORD& gameProcessID )
{
  GetWindowThreadProcessId( gameWindow, &gameProcessID );

  DWORD currentProcessIntegrity = GetProcessIntegrityLevel( GetCurrentProcess() );
  DWORD gw2ProcessIntegrity = GetProcessIntegrityLevel( OpenProcess( PROCESS_QUERY_INFORMATION, TRUE, gameProcessID ) );

  LOG_DBG( "[GW2TacO] Taco integrity: %x, GW2 integrity: %x", currentProcessIntegrity, gw2ProcessIntegrity );

  if ( gw2ProcessIntegrity > currentProcessIntegrity || gw2ProcessIntegrity == -1 )
    MessageBox( NULL, "GW2 seems to have more elevated rights than GW2 TacO.\nThis will probably result in TacO not being interactive when GW2 is in focus.\nIf this is an issue for you, restart TacO in Administrator mode.", "Warning", MB_ICONWARNING );
}

void HandleGW2WindowPositionChange( CRect& tacoWindowPos, HWND hwnd )
{
  RECT gameClientRect;
  POINT topLeft{};
  GetClientRect( gameWindow, &gameClientRect );
  ClientToScreen( gameWindow, &topLeft );

  if ( gameClientRect.right - gameClientRect.left != tacoWindowPos.Width() ||
       gameClientRect.bottom - gameClientRect.top != tacoWindowPos.Height() ||
       topLeft.x != tacoWindowPos.x1 ||
       topLeft.y != tacoWindowPos.y1 )
  {
    LOG_NFO( "[GW2TacO] gw2 window size change: %d %d %d %d (%d %d)", gameClientRect.left, gameClientRect.top, gameClientRect.right, gameClientRect.bottom, gameClientRect.right - gameClientRect.left, gameClientRect.bottom - gameClientRect.top );

    bool sizeChanged = gameClientRect.right - gameClientRect.left != tacoWindowPos.Width() || gameClientRect.bottom - gameClientRect.top != tacoWindowPos.Height();
    tacoWindowPos = CRect( gameClientRect.left + topLeft.x,
                           gameClientRect.top + topLeft.y,
                           gameClientRect.left + topLeft.x + gameClientRect.right - gameClientRect.left,
                           gameClientRect.top + topLeft.y + gameClientRect.bottom - gameClientRect.top );

    ::SetWindowPos( hwnd, 0, tacoWindowPos.x1, tacoWindowPos.y1, tacoWindowPos.Width(), tacoWindowPos.Height(), SWP_NOREPOSITION );

    if ( sizeChanged )
    {
      App->HandleResize();
      MARGINS marg = { -1, -1, -1, -1 };
      DwmExtendFrameIntoClientArea( hwnd, &marg );
    }
  }
}

void LoadQueuedMarkerPacks()
{
  if ( markerPackQueue.NumItems() )
  {
    CString file = markerPackQueue[ 0 ];
    markerPackQueue.DeleteByIndex( 0 );
    ImportMarkerPack( App, file );
  }
}

void CheckIfShutdownNeeded( TBOOL foundGameWindow, DWORD gameProcessID )
{
  // check to close with gw2
  if ( Config::GetValue( "CloseWithGW2" ) )
    if ( !gameWindow && foundGameWindow )
      if ( !IsProcessRunning( gameProcessID ) )
        App->SetDone( true );

  if ( !foundGameWindow )
  {
    if ( !mumbleLink.IsValid() && globalTimer.GetTime() > 60000 )
    {
      LOG_ERR( "[GW2TacO] Closing TacO because GW2 with mumble link '%s' was not found in under a minute", mumbleLink.mumblePath.GetPointer() );
      App->SetDone( true );
    }
  }
}

void WaitForMumble( bool frameThrottling )
{
  for ( int x = 0; x < ( frameThrottling ? 32 : 1 ); x++ )
  {
    if ( mumbleLink.Update() )
      break;
    if ( frameThrottling )
      Sleep( 1 );
  }
}

void EnumGW2Windows()
{
  gw2WindowCount = 0;
  gameWindow = nullptr;
  gw2WindowFromPid = nullptr;
  EnumWindows( GW2WindowFromPIDFunction, mumbleLink.lastGW2ProcessID );
  gameWindow = gw2WindowFromPid;
}

INT WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow )
{
  ImmDisableIME( -1 );

  InitLogging();

  Config::Load();
  Config::InitDefaults();

  if ( !ProcessCommandLine() )
    return 0;

  Localization::Import();

  if ( !InitOverlayApp( hInstance ) )
    return 0;

  UpdateMarkerPackList();

  HWND tacoHWND = (HWND)App->GetHandle();
  ShowWindow( tacoHWND, nCmdShow );

  ImportPOIS( App );
  mumbleLink.Update();

  extern WBATLASHANDLE DefaultIconHandle;
  if ( DefaultIconHandle == -1 )
  {
    auto skinItem = App->GetSkin()->GetElementID( CString( "defaulticon" ) );
    DefaultIconHandle = App->GetSkin()->GetElement( skinItem )->GetHandle();
  }

  ImportPOIActivationData();
  ImportLocationalTimers();
  LoadWvWObjectives();
  OpenOverlayWindows( App );
  CheckForNewTacOBuild();

  TBOOL foundGameWindow = false;
  CRect tacoWindowPos{};

  DWORD gameProcessID = 0;
  GW2TacO* taco = (GW2TacO*)App->GetRoot()->FindChildByID( "tacoroot", "GW2TacO" );

  if ( !taco )
    return 0;

  taco->InitScriptEngines();

  bool frameThrottling = Config::GetValue( "FrameThrottling" ) != 0;
  bool hideOnLoadingScreens = Config::GetValue( "HideOnLoadingScreens" ) != 0;
  TU32 oncePerSecondEvent = globalTimer.GetTime();
  
  InitInputHooks();

  bool needsUIScaleSet = true;

  while ( App->HandleMessages() )
  {
#ifdef _DEBUG
    if ( GetAsyncKeyState( VK_F11 ) )
      break;
#endif

    LoadQueuedMarkerPacks();

    int lastMumbleMap = mumbleLink.mapID;

    WaitForMumble( frameThrottling );

    if ( lastMumbleMap != mumbleLink.mapID )
      MarkerDOM::ResetUndoBuffer();

    if ( !App->DeviceOK() )
    {
      LOG_ERR( "[GW2TacO] Device fail" );
      continue;
    }
    
    auto currTime = globalTimer.GetTime();

    if ( currTime - oncePerSecondEvent > 1000 )
    {
      // once per second checks
      CheckIfShutdownNeeded( foundGameWindow, gameProcessID );

      hideOnLoadingScreens = Config::GetValue( "HideOnLoadingScreens" ) != 0;
      EnumGW2Windows();

      oncePerSecondEvent = globalTimer.GetTime();
    }

    if ( !mumbleLink.IsValid() || !gameWindow )
    {
      Sleep( 1000 );
      continue;
    }

    bool shortTick = !hideOnLoadingScreens || ( currTime - mumbleLink.lastFrameTime ) < 333;

    if ( !gameWindow )
      continue;

    if ( !foundGameWindow )
      ValidateGW2ProcessIntegrity( gameProcessID );
    foundGameWindow = true;

    HandleGW2WindowPositionChange( tacoWindowPos, tacoHWND );
    if ( needsUIScaleSet && mumbleLink.IsValid() )
    {
      ChangeUIScale( mumbleLink.uiSize );
      needsUIScaleSet = false;
    }

    auto foregroundWindow = GetForegroundWindow();

    if ( foregroundWindow == gameWindow && App->GetFocusItem() && App->GetFocusItem()->InstanceOf( "textbox" ) )
    {
      SetForegroundWindow( tacoHWND );
      SetFocus( tacoHWND );
      ::SetWindowPos( tacoHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    }

    if ( foregroundWindow == tacoHWND && !( App->GetFocusItem() && App->GetFocusItem()->InstanceOf( "textbox" ) ) )
    {
      SetForegroundWindow( gameWindow );
      SetFocus( gameWindow );
    }


    bool editedButNotSelected = ( foregroundWindow != gameWindow && foregroundWindow != tacoHWND && App->GetFocusItem() && App->GetFocusItem()->InstanceOf( "textbox" ) );
    if ( editedButNotSelected )
      App->GetRoot()->SetFocus();

    if ( !( App->GetFocusItem() && App->GetFocusItem()->InstanceOf( "textbox" ) || editedButNotSelected ) )
    {
      HWND wnd = ::GetNextWindow( gameWindow, GW_HWNDPREV );
      if ( wnd != tacoHWND )
      {
        if ( wnd )
          ::SetWindowPos( tacoHWND, wnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        else
          ::SetWindowPos( tacoHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
      }

      App->GetRoot()->Hide( !shortTick );
    }

    taco->TickScriptEngines();
    Config::AutoSaveConfig();

    App->Display();
    App->UpdateControlKeyStates();
  }

  ShowWindow( tacoHWND, SW_HIDE );

  {
    extern LIGHTWEIGHT_CRITICALSECTION zipCritSec;
    CLightweightCriticalSection fileWrite( &zipCritSec );
    FlushZipDict();
  }

  ShutDownInputHooks();
  Config::Save();
  ShutDownWvWChecking();

  extern std::unordered_map<int, CDictionaryEnumerable<GUID, GW2Trail*>> trailSet;

  for ( auto& trails : trailSet )
  {
    for ( int x = 0; x < trails.second.NumItems(); x++ )
      delete trails.second.GetByIndex( x );
  }

  WaitForMarkerPackUpdate();

  //cleanup
  SAFEDELETE( App );
  
  return true;
}