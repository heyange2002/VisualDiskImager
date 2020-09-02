// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "pch.h"
#include "VisualDiskImager.h"
#include "VisualDiskImagerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

void Log(LogPriority nPriority, LPCTSTR szFormat, ...)
{
	va_list ap;
	va_start( ap, szFormat );
	LogV( nPriority, szFormat, ap );
	va_end( ap );
}

void Log(LogPriority nPriority, UINT nFormat, ...)
{
	va_list ap;
	va_start( ap, nFormat );
	LogV( nPriority, LoadString( nFormat ), ap );
	va_end( ap );
}

void LogV(LogPriority nPriority, LPCTSTR szFormat, va_list ap)
{
	CAutoPtr< CString > plog( new CString );
	plog->FormatV( szFormat, ap );

	if ( CWnd* pWnd = AfxGetMainWnd() )
	{
		pWnd->SendMessage( WM_LOG, nPriority, reinterpret_cast< LPARAM >( plog.Detach() ) );
	}
	else if ( theApp.m_pMainWnd )
	{
		theApp.m_pMainWnd->PostMessage( WM_LOG, nPriority, reinterpret_cast< LPARAM >( plog.Detach() ) );
	}
}

CString LoadString(UINT nID)
{
	CString str;
	VERIFY( str.LoadString( nID ) );
	return str;
}

CString FormatByteSize(LONGLONG nSize)
{
	CString str;
	StrFormatByteSize( nSize, str.GetBuffer( 32 ), 32 );
	str.ReleaseBuffer();
	return str;
}

CString GetErrorString(DWORD error)
{
	CString str;
	str.Format( _T("0x%08x "), error );

	LPTSTR errormessage = nullptr;
	if ( FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, error, 0, (LPTSTR)&errormessage, 0, nullptr ) )
	{
		str += errormessage;
		str.TrimRight( _T("\r\n") );
		LocalFree( errormessage );
	}

	return str;
}

// CVisualDiskImagerApp

BEGIN_MESSAGE_MAP(CVisualDiskImagerApp, CWinApp)
END_MESSAGE_MAP()

// CVisualDiskImagerApp construction

CVisualDiskImagerApp::CVisualDiskImagerApp()
{
}

// The one and only CVisualDiskImagerApp object

CVisualDiskImagerApp theApp;

// CVisualDiskImagerApp initialization

BOOL CVisualDiskImagerApp::InitInstance()
{
	const INITCOMMONCONTROLSEX InitCtrls = { sizeof( InitCtrls ), ICC_WIN95_CLASSES | ICC_USEREX_CLASSES };
	InitCommonControlsEx( &InitCtrls );

	VERIFY( SUCCEEDED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) ) );

	VERIFY( SUCCEEDED( CoInitializeSecurity( nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
		nullptr, EOAC_NONE, nullptr ) ) );

	CWinApp::InitInstance();

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerWindows ) );

	SetRegistryKey( AFX_IDS_COMPANY_NAME );

	// Disable system error pop-ups
	SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );

	CVisualDiskImagerDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}