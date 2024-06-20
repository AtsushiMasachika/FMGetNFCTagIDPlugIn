
#include "FMWrapper/FMXTypes.h"
#include "FMWrapper/FMXText.h"
#include "FMWrapper/FMXFixPt.h"
#include "FMWrapper/FMXData.h"
#include "FMWrapper/FMXCalcEngine.h"

#include <vector>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <winscard.h>

// Exported plug-in functions/script steps =================================================

static const char* kFMEX( "FMEX" );

enum { kFMEX_GetNFCID = 300, kFMEX_GetNFCIDMin = 1, kFMEX_GetNFCIDMax = 1 };
static const char* kFMEX_GetNFCIDName("GetNFCTagID");
static const char* kFMEX_GetNFCIDDefinition("GetNFCTagID( timeoutseconds )");
static const char* kFMEX_GetNFCIDDescription("Return the NFC Tag ID");

static FMX_PROC(fmx::errcode) Do_FMEX_GetNFCID( short /* funcId */, const fmx::ExprEnv& /* environment */, const fmx::DataVect& dataVect, fmx::Data& results )
{
	// fmx::errcode errorResult(1978);
	fmx::TextUniquePtr cardIDText;
	const fmx::Locale* cardLocale(&results.GetLocale());
	
	SCARDCONTEXT hContext; // Resource manager context
	SCARDHANDLE hCard;	   // Card context handle

	DWORD dwActiveProtocol = 0; // Established active protocol
	DWORD dwReaders = 1;
	LONG lResult;
	LPTSTR mszReaders = NULL;
	bool cardInserted = false;

	// Setting for reader state
	SCARD_READERSTATE readerState = { 0 };
	memset(&readerState, 0, sizeof(readerState));

	// APDU command to get the UID
	BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
	DWORD dwSendLength = sizeof(pbSendBuffer);
	BYTE pbRecvBuffer[256];
	DWORD dwRecvLength = sizeof(pbRecvBuffer);
	SCARD_IO_REQUEST pioSendPci;


	// Variable to store the user's choice
	float userChoice = 0.0;

	// Check if user input exists
	if (dataVect.Size() >= 1) {
		const fmx::Data& choiceData = dataVect.At(0); 
		userChoice = choiceData.GetAsNumber().AsFloat();

		// Check if integer
		if (userChoice != static_cast<int>(userChoice)) {
			cardIDText->AssignWide(L"Error:0 Invalid input. Please enter an integer value.");
			results.SetAsText(*cardIDText, *cardLocale);
			return 0;
		}
	}
	else {
		cardIDText->AssignWide(L"Error:1 No input detected.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	DWORD dwTimeout;							
	if (userChoice < 1 || userChoice > 10) {
		// If an invalid value is entered, select standard 5 seconds
		userChoice = 5;							
	}

	// Multiply by 1000 and convert to milliseconds
	dwTimeout = static_cast<DWORD>(userChoice * 1000);

	// Establish the context with the Smart Card service
	lResult = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (lResult != SCARD_S_SUCCESS) {
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:2 SCardEstablish failed.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	// Get the size needed for the reader list
	lResult = SCardListReaders(hContext, NULL, NULL, &dwReaders);
	if (lResult != SCARD_S_SUCCESS) {
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:3 Get the size needed for the reader list failed.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	// Allocate memory for the reader list
	mszReaders = (LPTSTR)malloc(sizeof(TCHAR) * (dwReaders + 1));

	if (mszReaders == NULL) {
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:4 Allocate memory for the reader list failed.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	// Retrieve the actual reader names using the got reader list
	lResult = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	if (lResult != SCARD_S_SUCCESS) {
		free(mszReaders);
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:5 SCardListReaders failed.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	readerState.szReader = mszReaders;
	readerState.dwCurrentState = SCARD_STATE_UNAWARE;
	
	ULONGLONG startTime;
	startTime = GetTickCount64();
 
	// Wait up to 10 seconds for card insertion event
	do {
		lResult = SCardGetStatusChange(hContext, dwTimeout, &readerState, 1);
		if (lResult == SCARD_S_SUCCESS && (readerState.dwEventState & SCARD_STATE_PRESENT)) {
			cardInserted = true;
			break; // Card inserted, exit loop.
		}
        ULONGLONG elapsedTime = GetTickCount64() - startTime;
		if (elapsedTime > dwTimeout) {
			break; // Exit loop on timeout
		}
	} while (!cardInserted);

	if (!cardInserted) {
		// Processing when the loop exits due to a timeout
		free(mszReaders);
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:6 No card was detected.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}
	
	// Attempt to connect to the first reader
	lResult = SCardConnectW(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	if (lResult != SCARD_S_SUCCESS) {
		// What to do in case of connection failure
		free(mszReaders);
		SCardDisconnect(hCard, SCARD_LEAVE_CARD);
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:7 SCardConnect failed.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	// Processing after successful connection (e.g., obtaining the card's UID)
	// Set pioSendPci according to SCARD_PROTOCOL_T0 or SCARD_PROTOCOL_T1
	switch (dwActiveProtocol) {
	case SCARD_PROTOCOL_T0:
		pioSendPci = *SCARD_PCI_T0;
		break;
	case SCARD_PROTOCOL_T1:
		pioSendPci = *SCARD_PCI_T1;
		break;
	default:
		// For unsupported protocol
		free(mszReaders);
		SCardDisconnect(hCard, SCARD_LEAVE_CARD);
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:8 Unsupported protocol.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}

	// Send commands to the card using SCardTransmit
	lResult = SCardTransmit(hCard, &pioSendPci, pbSendBuffer, dwSendLength, NULL, pbRecvBuffer, &dwRecvLength);
	if (lResult != SCARD_S_SUCCESS) {
		free(mszReaders);
		SCardDisconnect(hCard, SCARD_LEAVE_CARD);
		SCardReleaseContext(hContext);
		cardIDText->AssignWide(L"Error:9 Failed to transmit command to the card.");
		results.SetAsText(*cardIDText, *cardLocale);
		return 0;
	}
	else {
		// Assemble UID as string
		std::stringstream ss;
		// Exclude the last 2 bytes (status word)
		for (DWORD i = 0; i < dwRecvLength - 2; ++i) { // Exclude the last 2 bytes (status word)
			ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(pbRecvBuffer[i]);
		}
		std::string uidStr = ss.str();

		// Return results to FileMaker
		fmx::TextUniquePtr uidText;
		uidText->Assign(uidStr.c_str(), fmx::Text::kEncoding_UTF8);
		results.SetAsText(*uidText, *cardLocale);
	}

	free(mszReaders);
	SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	SCardReleaseContext(hContext);
	return 0;

}

// Do_PluginInit ===========================================================================

static fmx::ptrtype Do_PluginInit( fmx::int16 version )
{
	fmx::ptrtype                    result( static_cast<fmx::ptrtype>(kDoNotEnable) );
	const fmx::QuadCharUniquePtr    pluginID( kFMEX[0], kFMEX[1], kFMEX[2], kFMEX[3] );
	fmx::TextUniquePtr              name;
	fmx::TextUniquePtr              definition;
	fmx::TextUniquePtr              description;
	fmx::uint32                     flags( fmx::ExprEnv::kDisplayInAllDialogs | fmx::ExprEnv::kFutureCompatible );

	if (version >= k120ExtnVersion)
	{
		name->Assign(kFMEX_GetNFCIDName, fmx::Text::kEncoding_UTF8);
		definition->Assign(kFMEX_GetNFCIDDefinition, fmx::Text::kEncoding_UTF8);
		//description->Assign(kFMEX_GetNFCIDDescription, fmx::Text::kEncoding_UTF8);
		if (fmx::ExprEnv::RegisterExternalFunction(*pluginID, kFMEX_GetNFCID, *name, *definition, kFMEX_GetNFCIDMin, kFMEX_GetNFCIDMax, flags, Do_FMEX_GetNFCID) == 0)
		{
			result = kCurrentExtnVersion;
		}
	}
	else if (version == k110ExtnVersion)
	{
		name->Assign(kFMEX_GetNFCIDName, fmx::Text::kEncoding_UTF8);
		definition->Assign(kFMEX_GetNFCIDDefinition, fmx::Text::kEncoding_UTF8);
		//description->Assign(kFMEX_GetNFCIDDescription, fmx::Text::kEncoding_UTF8);
		if (fmx::ExprEnv::RegisterExternalFunction(*pluginID, kFMEX_GetNFCID, *name, *definition, kFMEX_GetNFCIDMin, kFMEX_GetNFCIDMax, flags, Do_FMEX_GetNFCID) == 0)
		{
			result = kCurrentExtnVersion;
		}
	}

	if (version >= k150ExtnVersion)
	{
		name->Assign(kFMEX_GetNFCIDName, fmx::Text::kEncoding_UTF8);
		definition->Assign(kFMEX_GetNFCIDDefinition, fmx::Text::kEncoding_UTF8);
		//description->Assign(kFMEX_GetNFCIDDescription, fmx::Text::kEncoding_UTF8);
		if (fmx::ExprEnv::RegisterExternalFunction(*pluginID, kFMEX_GetNFCID, *name, *definition, kFMEX_GetNFCIDMin, kFMEX_GetNFCIDMax, flags, Do_FMEX_GetNFCID) == 0)
		{
			result = kCurrentExtnVersion;
		}
	}
	else if (version == k140ExtnVersion)
	{
		name->Assign(kFMEX_GetNFCIDName, fmx::Text::kEncoding_UTF8);
		definition->Assign(kFMEX_GetNFCIDDefinition, fmx::Text::kEncoding_UTF8);
		//description->Assign(kFMEX_GetNFCIDDescription, fmx::Text::kEncoding_UTF8);
		if (fmx::ExprEnv::RegisterExternalFunction(*pluginID, kFMEX_GetNFCID, *name, *definition, kFMEX_GetNFCIDMin, kFMEX_GetNFCIDMax, flags, Do_FMEX_GetNFCID) == 0)
		{
			result = kCurrentExtnVersion;
		}
	}

	if (version >= k160ExtnVersion)
	{
		name->Assign(kFMEX_GetNFCIDName, fmx::Text::kEncoding_UTF8);
		definition->Assign(kFMEX_GetNFCIDDefinition, fmx::Text::kEncoding_UTF8);
		//description->Assign(kFMEX_GetNFCIDDescription, fmx::Text::kEncoding_UTF8);
		if (fmx::ExprEnv::RegisterExternalFunction(*pluginID, kFMEX_GetNFCID, *name, *definition, kFMEX_GetNFCIDMin, kFMEX_GetNFCIDMax, flags, Do_FMEX_GetNFCID) == 0)
		{
			result = kCurrentExtnVersion;
		}
	}
	 
	return result;
}

// Do_PluginShutdown =======================================================================

static void Do_PluginShutdown( fmx::int16 version )
{
	const fmx::QuadCharUniquePtr    pluginID( kFMEX[0], kFMEX[1], kFMEX[2], kFMEX[3] );

	if (version >= k120ExtnVersion)
	{
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction(*pluginID, kFMEX_GetNFCID));
	}

	if (version >= k140ExtnVersion)
	{
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction(*pluginID, kFMEX_GetNFCID));
	}

	if (version >= k160ExtnVersion)
	{
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction( *pluginID, kFMEX_GetNFCID ));
	}
	
}

// Do_GetString ============================================================================

static void CopyUTF8StrToUnichar16Str( const char* inStr, fmx::uint32 outStrSize, fmx::unichar16* outStr )
{
	fmx::TextUniquePtr txt;
	txt->Assign( inStr, fmx::Text::kEncoding_UTF8 );
	const fmx::uint32 txtSize( (outStrSize <= txt->GetSize()) ? (outStrSize - 1) : txt->GetSize() );
	txt->GetUnicode( outStr, 0, txtSize );
	outStr[txtSize] = 0;
}

static void Do_GetString( fmx::uint32 whichString, fmx::uint32 /* winLangID */, fmx::uint32 outBufferSize, fmx::unichar16* outBuffer )
{
	switch (whichString)
	{
		case kFMXT_NameStr:
		{
			CopyUTF8StrToUnichar16Str( "GetNFCTagIDPlugIn", outBufferSize, outBuffer );
			break;
		}
			
		case kFMXT_AppConfigStr:
		{
			CopyUTF8StrToUnichar16Str( "Get NFC Tag ID Plug-In From FileMaker", outBufferSize, outBuffer );
			break;
		}
			
		case kFMXT_OptionsStr:
		{
			// Characters 1-4 are the plug-in ID
			CopyUTF8StrToUnichar16Str( kFMEX, outBufferSize, outBuffer );

			// Character 5 is always "1"
			outBuffer[4] = '1';
			
			// Character 6
			// use "Y" if you want to enable the Configure button for plug-ins in the Preferences dialog box.
			// Use "n" if there is no plug-in configuration needed.
			// If the flag is set to "Y" then make sure to handle the kFMXT_DoAppPreferences message.
			outBuffer[5] = 'n';
			
			// Character 7  is always "n"
			outBuffer[6] = 'n';
			
			// Character 8
			// Set to "Y" if you want to receive kFMXT_Init/kFMXT_Shutdown messages
			// In most cases, you want to set it to 'Y' since it's the best time to register/unregister your plugin functions.
			outBuffer[7] = 'Y';
			
			// Character 9
			// Set to "Y" if the kFMXT_Idle message is required.
			// For simple external functions this may not be needed and can be turned off by setting the character to "n"
			outBuffer[8] = 'n';
			
			// Character 10
			// Set to "Y" to receive kFMXT_SessionShutdown and kFMXT_FileShutdown messages
			outBuffer[9] = 'n';
			
			// Character 11 is always "n"
			outBuffer[10] = 'n';
			
			// NULL terminator
			outBuffer[11] = 0;
			break;
		}

		case kFMXT_HelpURLStr:
		{
			CopyUTF8StrToUnichar16Str( "http://httpbin.org/get?id=", outBufferSize, outBuffer );
			break;
		}

		default:
		{
			outBuffer[0] = 0;
			break;
		}
	}
}

// Do_PluginIdle ===========================================================================

static void Do_PluginIdle( FMX_IdleLevel idleLevel, fmx::ptrtype /* sessionId */ )
{
	// Check idle state
	switch (idleLevel)
	{
		case kFMXT_UserIdle:
		case kFMXT_UserNotIdle:
		case kFMXT_ScriptPaused:
		case kFMXT_ScriptRunning:
		case kFMXT_Unsafe:
		default:
		{
			break;
		}
	}
}

// Do_PluginPrefs ==========================================================================

static void Do_PluginPrefs( void )
{
}

// Do_SessionNotifications =================================================================

static void Do_SessionNotifications( fmx::ptrtype /* sessionID */ )
{
}

// Do_FileNotifications ====================================================================

static void Do_FilenNotifications( fmx::ptrtype /* sessionID */, fmx::ptrtype /* fileID */ )
{
}

// FMExternCallProc ========================================================================

FMX_ExternCallPtr gFMX_ExternCallPtr( nullptr );

void FMX_ENTRYPT FMExternCallProc( FMX_ExternCallPtr pb )
{
	// Setup global defined in FMXExtern.h (this will be obsoleted in a later header file)
	gFMX_ExternCallPtr = pb;
	
	// Message dispatcher
	switch (pb->whichCall)
	{
		case kFMXT_Init:
			pb->result = Do_PluginInit( pb->extnVersion );
			break;
			
		case kFMXT_Idle:
			Do_PluginIdle( pb->parm1, pb->parm2 );
			break;
			
		case kFMXT_Shutdown:
			Do_PluginShutdown( pb->extnVersion );
			break;
			
		case kFMXT_DoAppPreferences:
			Do_PluginPrefs();
			break;
			
		case kFMXT_GetString:
			Do_GetString( static_cast<fmx::uint32>(pb->parm1), static_cast<fmx::uint32>(pb->parm2), static_cast<fmx::uint32>(pb->parm3), reinterpret_cast<fmx::unichar16*>(pb->result) );
			break;
			
		case kFMXT_SessionShutdown:
			Do_SessionNotifications( pb->parm2 );
			break;

		case kFMXT_FileShutdown:
			Do_FilenNotifications( pb->parm2, pb->parm3 );
			break;
			
	} // switch whichCall

} // FMExternCallProc
