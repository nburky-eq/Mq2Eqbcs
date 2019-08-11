#pragma comment(lib,"wsock32.lib")
#include "../MQ2Plugin.h"
#include <vector>
const char*        MODULE_NAME        = "MQ2EQBC";
const double       MODULE_VERSION     = 11.0219;
PreSetup(MODULE_NAME);
PLUGIN_VERSION(MODULE_VERSION);

// --------------------------------------
// constants
const char*        PROG_VERSION       = "MQ2EQBC 11.0219";
const char*        CONNECT_START      = "LOGIN";
const char*        CONNECT_START2     = "=";
const char*        CONNECT_END        = ";";
const char*        CONNECT_PWSEP      = ":";
const char*        SEND_LINE_TERM     = "\n";
const unsigned int MAX_READBUF        = 512;
const unsigned int COMMAND_HIST_SIZE  = 50;
const int          MAX_PASSWORD       = 40; // do not change without checking out the cmd buffer in eqbcs

// consistency
#define COLOR_NAME "\ay"
#define COLOR_NAME_BRACKET "\ar"
#define COLOR_OFF "\ax"
#define COLOR_STELL1 "\ax\ar[(\ax\aymsg\ax\ar)\ax\ay"
#define COLOR_STELL2 "\ax\ar]\ax "

// eqbcs msg types
#define CMD_DISCONNECT "\tDISCONNECT\n"
#define CMD_NAMES "\tNAMES\n"
#define CMD_PONG "\tPONG\n"
#define CMD_MSGALL "\tMSGALL\n"
#define CMD_TELL "\tTELL\n"
#define CMD_CHANNELS "\tCHANNELS\n"
#define CMD_LOCALECHO "\tLOCALECHO "
#define CMD_BCI "\tBCI\n"

// commands & settings
const char* VALID_COMMANDS     = "connect quit help status reconnect names version colordump channels stopreconnect forceconnect iniconnect";
const char* VALID_SETTINGS     = "autoconnect control compatmode window reconnectsecs localecho tellwatch guildwatch groupwatch fswatch silentcmd savebychar silentinccmd notifycontrol silentoutmsg echoall";
const char* szCmdConnect       = "connect";
const char* szCmdDisconnect    = "quit";
const char* szCmdHelp          = "help";
const char* szCmdStatus        = "status";
const char* szCmdReconnect     = "reconnect";
const char* szCmdNames         = "names";
const char* szCmdRelog         = "relog";
const char* szCmdVersion       = "version";
const char* szCmdColorDump     = "colordump";
const char* szCmdChannels      = "channels";
const char* szCmdNoReconnect   = "stopreconnect";
const char* szCmdForceConnect  = "forceconnect";
const char* szCmdIniConnect    = "iniconnect";
const char* szSetReconnect     = "reconnectsecs";
const char* szSetAutoConnect   = "autoconnect";
const char* szSetControl       = "control";
const char* szSetCompatMode    = "compatmode";
const char* szSetAutoReconnect = "reconnect";
const char* szSetWindow        = "window";
const char* szSetLocalEcho     = "localecho";
const char* szSetTellWatch     = "tellwatch";
const char* szSetGuildWatch    = "guildwatch";
const char* szSetGroupWatch    = "groupwatch";
const char* szSetFSWatch       = "fswatch";
const char* szSetSilentCmd     = "silentcmd";
const char* szSetSaveByChar    = "savebychar";
const char* szSetSilentIncCmd  = "silentinccmd";
const char* szSetSilentOutMsg  = "silentoutmsg";
const char* szSetNotifyControl = "notifycontrol";
const char* szSetEchoAll       = "echoall";

// --------------------------------------
// strings
char szPassword[MAX_PASSWORD]   = {0};
char szServer[MAX_STRING]       = {0};
char szPort[MAX_STRING]         = {0};
char szToonName[MAX_STRING]     = {0};
char szCharName[MAX_STRING]     = {0};
char szToonCmdStart[MAX_STRING] = {0};
char szColorChars[]             = "yogurtbmpwx";

// --------------------------------------
// winsock

int                iNRet = 0;
CRITICAL_SECTION   ConnectCS;
sockaddr_in        serverInfo;
SOCKET             theSocket;

// --------------------------------------
// class instances
class EQBCType*        pEQBCType = NULL;
class CSettingsMgr*          SET = NULL;
class CEQBCWndHandler*    WINDOW = NULL;
class CConnectionMgr*       EQBC = NULL;

// --------------------------------------
// function prototypes
typedef void (__cdecl *fNetBotOnMsg)(char*, char*);
typedef void (__cdecl *fNetBotOnEvent)(char*);
void WriteOut(char* szText);
unsigned long __stdcall EQBCConnectThread(void* lpParam);

// --------------------------------------
// utility functions

inline bool ValidIngame()
{
    // CTD prevention function
    PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
    if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer || !pChSpawn->SpawnID)
    {
        return false;
    }
    return true;
}

inline char* GetCharName()
{
    PCHARINFO pChar = GetCharInfo();
    if (GetGameState() != GAMESTATE_INGAME || !pChar || !pChar->Name) return NULL;
    return pChar->Name;
}

inline void SetPlayer()
{
    char* pszName = GetCharName();
    strcpy(szToonName, pszName ? pszName : "YouPlayer");
    sprintf(szToonCmdStart, "%s //", szToonName);
}

// --------------------------------------
// Configuration

class CSettingsMgr
{
public:
    int AllowControl;
    int AutoConnect;
    int AutoReconnect;
    int CustTitle;
    int IRCMode;
    int EchoAll;
    int LocalEcho;
    int NotifyControl;
    int ReconnectSecs;
    int SaveByChar;
    int SetTitle;
    int SilentCmd;
    int SilentIncCmd;
    int SilentOutMsg;
    int WatchFsay;
    int WatchGroup;
    int WatchGuild;
    int WatchTell;
    int Window;

    char WndKey[MAX_STRING];
    bool Loaded;
    bool FirstLoad;

    void LoadINI()
    {
        char szTemp[MAX_STRING] = {0};
        GetPrivateProfileString("Last Connect", "Server", "127.0.0.1", szServer, MAX_STRING, INIFileName);
        GetPrivateProfileString("Last Connect", "Port",   "2112",        szPort, MAX_STRING, INIFileName);
        AllowControl  = GetPrivateProfileInt("Settings", "AllowControl",           1, INIFileName);
        AutoConnect   = GetPrivateProfileInt("Settings", "AutoConnect",            0, INIFileName);
        AutoReconnect = GetPrivateProfileInt("Settings", "AutoReconnect",          1, INIFileName);
        IRCMode       = GetPrivateProfileInt("Settings", "IRCCompatMode",          1, INIFileName);
        LocalEcho     = GetPrivateProfileInt("Settings", "LocalEcho",              1, INIFileName);
        EchoAll       = GetPrivateProfileInt("Settings", "EchoAll",                0, INIFileName);
        NotifyControl = GetPrivateProfileInt("Settings", "NotifyControl",          0, INIFileName);
        ReconnectSecs = GetPrivateProfileInt("Settings", "ReconnectRetrySeconds", 15, INIFileName);
        SaveByChar    = GetPrivateProfileInt("Settings", "SaveByCharacter",        1, INIFileName);
        SilentCmd     = GetPrivateProfileInt("Settings", "SilentCmd",              0, INIFileName);
        SilentIncCmd  = GetPrivateProfileInt("Settings", "SilentIncCmd",           0, INIFileName);
        SilentOutMsg  = GetPrivateProfileInt("Settings", "SilentOutMsg",           0, INIFileName);
        WatchFsay     = GetPrivateProfileInt("Settings", "FSWatch",                0, INIFileName);
        WatchGroup    = GetPrivateProfileInt("Settings", "GroupWatch",             0, INIFileName);
        WatchGuild    = GetPrivateProfileInt("Settings", "GuildWatch",             0, INIFileName);
        WatchTell     = GetPrivateProfileInt("Settings", "TellWatch",              0, INIFileName);
        Window        = GetPrivateProfileInt("Settings", "UseWindow",              0, INIFileName);

        GetPrivateProfileString("Settings", "Keybind", "~", WndKey, MAX_STRING, INIFileName);
        KeyCombo Combo;
        ParseKeyCombo(WndKey, Combo);
        SetMQ2KeyBind("EQBC", FALSE, Combo);
        Loaded = true;
    };

    void UpdateServer()
    {
        WritePrivateProfileString("Last Connect", "Server", szServer, INIFileName);
        WritePrivateProfileString("Last Connect", "Port",     szPort, INIFileName);
    };

    void Change(char* szSetting, bool bToggle)
    {
        char szMsg[MAX_STRING]   = {0};
        char szArg[MAX_STRING] = {0};
        char szState[MAX_STRING] = {0};
        bool bFailed             = false;
        int  bTurnOn             = FALSE;

        GetArg(szArg, szSetting, 1);
        if (!*szArg)
        {
            bFailed = true;
        }
        else if (!strnicmp(szArg, szSetReconnect, sizeof(szSetReconnect)))
        {
            char szDigit[MAX_STRING] = {0};
            GetArg(szDigit, szSetting, 2);
            if (*szDigit && atoi(szDigit) > 0)
            {
                ReconnectSecs = atoi(szDigit);
                sprintf(szMsg, "%d", ReconnectSecs);
                WritePrivateProfileString("Settings", "ReconnectRetrySeconds", szMsg, INIFileName);
                sprintf(szMsg, "\ar#\ax Will now try to reconnect every %d seconds after server disconnect.", ReconnectSecs);
            }
            else
            {
                sprintf(szMsg, "\ar#\ax Invalid value given - proper example: /bccmd set reconnectsecs 15");
            }
            WriteOut(szMsg);
            return;
        }

        if (!bFailed && !bToggle)
        {
            GetArg(szState, szSetting, 2);
            if (!strnicmp(szState, "on", 3))
            {
                bTurnOn = TRUE;
            }
            else if (!strnicmp(szState, "off", 4))
            {
                bTurnOn = FALSE; // validate input
            }
            else
            {
                sprintf(szMsg, "\ar#\ax Invalid 'set %s' syntax (\ar%s\ax) -- Use [ \ayon\ax | \ayoff\ax ]", szArg, szState);
                WriteOut(szMsg);
                return;
            }
        }

        if (!bFailed)
        {
            if (!strnicmp(szArg, szSetAutoConnect, sizeof(szArg)))
            {
                ToggleSetting(&AutoConnect, &bToggle, &bTurnOn, "AutoConnect", "Auto Connect");
            }
            else if (!strnicmp(szArg, szSetControl, sizeof(szArg)))
            {
                ToggleSetting(&AllowControl, &bToggle, &bTurnOn, "AllowControl", "Allow Control");
            }
            else if (!strnicmp(szArg, szSetCompatMode, sizeof(szArg)))
            {
                ToggleSetting(&IRCMode, &bToggle, &bTurnOn, "IRCCompatMode", "IRC Compat Mode");
            }
            else if (!strnicmp(szArg, szSetReconnect, sizeof(szArg)))
            {
                ToggleSetting(&AutoReconnect, &bToggle, &bTurnOn, "AutoReconnect", "Auto Reconnect (on remote disconnect)");
            }
            else if (!strnicmp(szArg, szSetWindow, sizeof(szArg)))
            {
                if (ToggleSetting(&Window, &bToggle, &bTurnOn, "UseWindow", "Use Dedicated Window"))
                {
                    HandleWnd(true);
                }
                else
                {
                    HandleWnd(false);
                }
            }
            else if (!strnicmp(szArg, szSetLocalEcho, sizeof(szArg)))
            {
                ToggleSetting(&LocalEcho, &bToggle, &bTurnOn, "LocalEcho", "Echo my channel commands back to me");
                HandleEcho();
            }
            else if (!strnicmp(szArg, szSetEchoAll, sizeof(szArg)))
            {
                ToggleSetting(&EchoAll, &bToggle, &bTurnOn, "EchoAll", "Echo outgoing /bca messages");
            }
            else if (!strnicmp(szArg, szSetFSWatch, sizeof(szArg)))
            {
                ToggleSetting(&WatchFsay, &bToggle, &bTurnOn, "FSWatch", "Relay fellowship chat to /bc");
            }
            else if (!strnicmp(szArg, szSetGroupWatch, sizeof(szSetGroupWatch)))
            {
                ToggleSetting(&WatchGroup, &bToggle, &bTurnOn, "GroupWatch", "Relay group chat to /bc");
            }
            else if (!strnicmp(szArg, szSetGuildWatch, sizeof(szArg)))
            {
                ToggleSetting(&WatchGuild, &bToggle, &bTurnOn, "GuildWatch", "Relay guild chat to /bc");
            }
            else if (!strnicmp(szArg, szSetTellWatch, sizeof(szArg)))
            {
                ToggleSetting(&WatchTell, &bToggle, &bTurnOn, "TellWatch", "Relay all tells to /bc");
            }
            else if (!strnicmp(szArg, szSetSaveByChar, sizeof(szArg)))
            {
                ToggleSetting(&SaveByChar, &bToggle, &bTurnOn, "SaveByCharacter", "Save UI data by character name");
            }
            else if (!strnicmp(szArg, szSetSilentCmd, sizeof(szArg)))
            {
                ToggleSetting(&SilentCmd, &bToggle, &bTurnOn, "SilentCmd", "Silence 'CMD: [command]' echo");
            }
            else if (!strnicmp(szArg, szSetSilentIncCmd, sizeof(szArg)))
            {
                ToggleSetting(&SilentIncCmd, &bToggle, &bTurnOn, "SilentIncCmd", "Silence incoming commands");
            }
            else if (!strnicmp(szArg, szSetSilentOutMsg, sizeof(szArg)))
            {
                ToggleSetting(&SilentOutMsg, &bToggle, &bTurnOn, "SilentOutMsg", "Silence outgoing msgs");
            }
            else if (!strnicmp(szArg, szSetNotifyControl, sizeof(szArg)))
            {
                ToggleSetting(&NotifyControl, &bToggle, &bTurnOn, "NotifyControl", "Notify /bc when disabled control blocks incoming command");
            }
            else
            {
                bFailed = true;
            }
        }

        if (!bFailed) return;
        sprintf(szMsg, "\ar#\ax Unsupported parameter. Valid options: %s", VALID_SETTINGS);
        WriteOut(szMsg);
    };

    CSettingsMgr()
    {
        AllowControl  = 1;
        AutoConnect   = 0;
        AutoReconnect = 1;
        CustTitle     = 0;
        EchoAll       = 0;
        IRCMode       = 1;
        LocalEcho     = 1;
        NotifyControl = 0;
        ReconnectSecs = 15;
        SaveByChar    = 1;
        SetTitle      = 0;
        SilentCmd     = 0;
        SilentIncCmd  = 0;
        SilentOutMsg  = 0;
        WatchFsay     = 0;
        WatchGroup    = 0;
        WatchGuild    = 0;
        WatchTell     = 0;
        Window        = 0;
        memset(&WndKey, 0, MAX_STRING);
        FirstLoad     = false;
        Loaded        = false;
    };

private:
    int ToggleSetting(int* pbOption, bool* pbToggle, int* pbTurnOn, char* szOptName, char* szOptDesc)
    {
        char szTemp[MAX_STRING] = {0};
        *pbOption = *pbToggle ? (*pbOption ? FALSE : TRUE) : *pbTurnOn;
        sprintf(szTemp,"\ar#\ax Setting: %s turned %s", szOptDesc, (*pbOption) ? "ON" : "OFF");
        WriteOut(szTemp);
        sprintf(szTemp, "%d", *pbOption);
        WritePrivateProfileString("Settings", szOptName, szTemp, INIFileName);
        return *pbOption;
    };

    void HandleWnd(bool);
    void HandleEcho();
};

// --------------------------------------
// Custom UI Window

class CEQBCWnd : public CCustomWnd
{
public:
    CTextEntryWnd*     InputBox;
    CStmlWnd*          StmlOut;
    CXWnd*             OutWnd;
    struct _CSIDLWND*  OutStruct;

    CEQBCWnd(CXStr* Template) : CCustomWnd(Template)
    {
        iCurCommand            = -1;
        SetWndNotification(CEQBCWnd);
        StmlOut                = (CStmlWnd *)GetChildItem("CWChatOutput");
        OutWnd                 = (CXWnd*)StmlOut;
        OutWnd->Clickable      = 1;
        OutStruct              = (_CSIDLWND *)GetChildItem("CWChatOutput");
        InputBox               = (CTextEntryWnd*)GetChildItem("CWChatInput");
        InputBox->WindowStyle |= 0x800C0;
        InputBox->UnknownCW   |= 0xFFFFFFFF;
        CloseOnESC             = 0;
        InputBox->SetMaxChars(512);
        BitOff(WindowStyle, CWS_CLOSE);
        *(unsigned long*)&(((char*)StmlOut)[EQ_CHAT_HISTORY_OFFSET]) = 400;
    };

    int WndNotification(CXWnd* pWnd, unsigned int uiMessage, void* pData)
    {
        //static char szBCAA[] = "/bcaa "; // trailing space intentional
        if (pWnd == (CXWnd*)InputBox)
        {
            if (uiMessage == XWM_HITENTER)
            {
                char szBuffer[513] = {0};
                GetCXStr((PCXSTR)InputBox->InputText, szBuffer, 512);
                if (szBuffer[0])
                {
                    if (!sCmdHistory.size() || sCmdHistory.front().compare(szBuffer))
                    {
                        if (sCmdHistory.size() > COMMAND_HIST_SIZE)
                        {
                            sCmdHistory.pop_back();
                        }
                        sCmdHistory.insert(sCmdHistory.begin(), string(szBuffer));
                    }
                    iCurCommand = -1;
                    SetCXStr(&InputBox->InputText, "");
                    if (szBuffer[0] == '/')
                    {
                        DoCommand((PSPAWNINFO)pLocalPlayer, szBuffer);
                    }
                    else
                    {
                        WriteToBC(szBuffer);
                    }
                }
                ((CXWnd*)InputBox)->ClrFocus();
                ResetKeybinds();
            }
            else if (uiMessage == XWM_HISTORY && pData)
            {
                int* pInt      = (int*)pData;
                int  iKeyPress = pInt[1];
                if (iKeyPress == 200) // KeyUp: 0xC8
                {
                    if (sCmdHistory.size() > 0)
                    {
                        iCurCommand++;
                        if (iCurCommand < (int)sCmdHistory.size() && iCurCommand >= 0)
                        {
                            string s = (string)sCmdHistory.at(iCurCommand);
                            ((CXWnd*)InputBox)->SetWindowTextA(CXStr(s.c_str()));
                        }
                        else
                        {
                            iCurCommand = (int)sCmdHistory.size() - 1;
                        }
                    }
                    ResetKeybinds();
                }
                else if (iKeyPress == 208) // KeyDown: 0xD0
                {
                    if (sCmdHistory.size() > 0)
                    {
                        iCurCommand--;
                        if (iCurCommand >= 0 && sCmdHistory.size() > 0)
                        {
                            string s = (string)sCmdHistory.at(iCurCommand);
                            ((CXWnd*)InputBox)->SetWindowTextA(CXStr(s.c_str()));
                        }
                        else if (iCurCommand < 0)
                        {
                            iCurCommand = -1;
                            // Hit bottom.
                            SetCXStr(&InputBox->InputText, "");
                        }
                    }
                    ResetKeybinds();
                }
            }
        }
        else if (pWnd == NULL && uiMessage == XWM_CLOSE)
        {
            dShow = 1;
            ResetKeybinds();
            return 0;
        }
        else if (uiMessage == XWM_LINK)
        {
            class CChatWindow* p = (class CChatWindow*)this;
            if (StmlOut != (CStmlWnd*)pWnd)
            {
                CStmlWnd* pTmp = NULL;
                int iRet = 0;
                pTmp = StmlOut;
                StmlOut = (CStmlWnd*)pWnd;
                iRet = p->WndNotification(pWnd, uiMessage, pData);
                StmlOut = pTmp;
                return iRet;
            }
            return p->WndNotification(pWnd, uiMessage, pData);
        }
        return CSidlScreenWnd::WndNotification(pWnd, uiMessage, pData);
    };
private:
    vector<string> sCmdHistory;
    int            iCurCommand;

    void WriteToBC(char*);
    void ResetKeybinds();
};

class CEQBCWndHandler
{
public:
    void Create()
    {
        if (!SET->Window || !ValidIngame() || BCWnd) return;
        NewWnd();
    };

    void Destroy(bool bSave)
    {
        if (!BCWnd) return;
        if (bSave) SaveWnd();
        delete BCWnd;
        BCWnd = NULL;
    };

    void Clear()
    {
        if (!BCWnd) return;
        ((CChatWindow*)BCWnd)->Clear();
    };

    void Hover()
    {
        if (!BCWnd) return;
        ((CXWnd*)BCWnd)->DoAllDrawing();
    };

    void Min()
    {
        if (!BCWnd) return;
        ((CXWnd*)BCWnd)->OnMinimizeBox();
    };

    void Save()
    {
        if (!BCWnd) return;
        SaveWnd();
    };

    void Write(char* szText)
    {
        if (!BCWnd) return;
        Output(szText);
    };

    void NewFont(int iSize)
    {
        if (!BCWnd || iSize < 0) return;
        SetFontSize(iSize);
    };

    void UpdateTitle()
    {
        if (!BCWnd || SET->CustTitle) return;
        char szWindowText[MAX_STRING] = {0};
        GetCXStr(BCWnd->WindowText, szWindowText);
        if (strnicmp(szWindowText, szServer, sizeof(szWindowText)))
        {
            SetCXStr(&BCWnd->WindowText, szServer);
        }
    };

    void Keybind(int iDown)
    {
        if (!ValidIngame() || !BCWnd) return;
        if (iDown)
        {
            if (!KeyActive)
            {
                CXRect rect = ((CXWnd*)BCWnd->InputBox)->GetScreenRect();
                CXPoint pt  = rect.CenterPoint();
                ((CXWnd*)BCWnd->InputBox)->SetWindowTextA(CXStr(""));
                ((CXWnd*)BCWnd->InputBox)->HandleLButtonDown(&pt, 0);
                KeyActive = true;
                return;
            }
            SetCXStr(&BCWnd->InputBox->InputText, "");
            ((CXWnd*)BCWnd->InputBox)->ClrFocus();
            KeyActive = false;
        }
    };

    void ResetKeys()
    {
        KeyActive = false;
    };

    CEQBCWndHandler()
    {
        BCWnd     = NULL;
        FontSize  = 4;
        KeyActive = false;
    };
private:
    void NewWnd()
    {
        char szWindowText[MAX_STRING] = {0};
        sprintf(szWindowText, "%s", szServer);
        class CXStr ChatWnd("ChatWindow");
        BCWnd = new CEQBCWnd(&ChatWnd);

        SET->CustTitle         = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "UseMyTitle",   0,    INIFileName);
        BCWnd->Location.top    = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "ChatTop",      10,   INIFileName);
        BCWnd->Location.bottom = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "ChatBottom",   210,  INIFileName);
        BCWnd->Location.left   = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "ChatLeft",     10,   INIFileName);
        BCWnd->Location.right  = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "ChatRight",    410,  INIFileName);
        BCWnd->Fades           = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "Fades",        0,    INIFileName);
        BCWnd->Alpha           = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "Alpha",        255,  INIFileName);
        BCWnd->FadeToAlpha     = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "FadeToAlpha",  255,  INIFileName);
        BCWnd->FadeDuration    = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "Duration",     500,  INIFileName);
        BCWnd->Locked          = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "Locked",       0,    INIFileName);
        BCWnd->TimeMouseOver   = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "Delay",        2000, INIFileName);
        BCWnd->BGType          = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "BGType",       1,    INIFileName);
        BCWnd->BGColor.R       = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "BGTint.red",   255,  INIFileName);
        BCWnd->BGColor.G       = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "BGTint.green", 255,  INIFileName);
        BCWnd->BGColor.B       = GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "BGTint.blue",  255,  INIFileName);

        NewFont(GetPrivateProfileInt(SET->SaveByChar ? szCharName : "Window", "FontSize", 4, INIFileName));
        if (SET->CustTitle)
        {
            GetPrivateProfileString(SET->SaveByChar ? szCharName : "Window", "WindowTitle", szWindowText, szWindowText, MAX_STRING, INIFileName);
        }
        SetCXStr(&BCWnd->WindowText, szWindowText);
        ((CXWnd*)BCWnd)->Show(1, 1);
        BitOff(BCWnd->OutStruct->WindowStyle, CWS_CLOSE);
    };

    void SaveWnd()
    {
        PCSIDLWND UseWnd = (PCSIDLWND)BCWnd;
        char szTemp[200]              = {0};

        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "ChatTop",      itoa(UseWnd->Location.top,    szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "ChatBottom",   itoa(UseWnd->Location.bottom, szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "ChatLeft",     itoa(UseWnd->Location.left,   szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "ChatRight",    itoa(UseWnd->Location.right,  szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "Fades",        itoa(UseWnd->Fades,           szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "Alpha",        itoa(UseWnd->Alpha,           szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "FadeToAlpha",  itoa(UseWnd->FadeToAlpha,     szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "Duration",     itoa(UseWnd->FadeDuration,    szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "Locked",       itoa(UseWnd->Locked,          szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "Delay",        itoa(UseWnd->TimeMouseOver,   szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "BGType",       itoa(UseWnd->BGType,          szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "BGTint.red",   itoa(UseWnd->BGColor.R,       szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "BGTint.green", itoa(UseWnd->BGColor.G,       szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "BGTint.blue",  itoa(UseWnd->BGColor.B,       szTemp, 10), INIFileName);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "FontSize",     itoa(FontSize,                szTemp, 10), INIFileName);
        GetCXStr(UseWnd->WindowText, szTemp);
        WritePrivateProfileString(SET->SaveByChar ? szCharName : "Window", "WindowTitle",                                szTemp,      INIFileName);
    };

    void Output(char* szText)
    {
        ((CXWnd*)BCWnd)->Show(1, 1);
        bool bScrollDown = (BCWnd->OutWnd->VScrollPos == BCWnd->OutWnd->VScrollMax) ? true : false;
        char szProcessed[MAX_STRING] = {0};
        StripMQChat(szText, szProcessed);
        CheckChatForEvent(szProcessed);
        MQToSTML(szText, szProcessed, MAX_STRING);
        strcat(szProcessed, "<br>");
        CXStr NewText(szProcessed);
        ConvertItemTags(NewText, TRUE);
        (BCWnd->StmlOut)->AppendSTML(NewText);
        if (bScrollDown) (BCWnd->OutWnd)->SetVScrollPos(BCWnd->OutStruct->VScrollMax);
    };

    void SetFontSize(unsigned int uiSize)
    {
        struct FONTDATA
        {
            unsigned long ulNumFonts;
            char**        ppFonts;
        };
        FONTDATA*      pFonts;       // font array structure
        unsigned long* pulSelFont;   // selected font
        pFonts = (FONTDATA*)&(((char*)pWndMgr)[EQ_CHAT_FONT_OFFSET]);
        if (!pFonts->ppFonts || uiSize >= (int)pFonts->ulNumFonts)
        {
            return;
        }
        pulSelFont = (unsigned long*)pFonts->ppFonts[uiSize];

        CXStr ContStr(((CStmlWnd*)BCWnd->StmlOut)->GetSTMLText());
        ((CXWnd*)   BCWnd->StmlOut)->SetFont(pulSelFont);
        ((CStmlWnd*)BCWnd->StmlOut)->SetSTMLText(ContStr, 1, 0);
        ((CStmlWnd*)BCWnd->StmlOut)->ForceParseNow();
        ((CXWnd*)   BCWnd->StmlOut)->SetVScrollPos(BCWnd->StmlOut->VScrollMax);

        FontSize = uiSize;
    };

    CEQBCWnd*     BCWnd;
    unsigned long FontSize;
    bool          KeyActive;
};

void WriteOut(char *szText)
{
    typedef int (__cdecl *fMQWriteBC)(char *szText);
    int bWrite        = true;
    PMQPLUGIN pPlugin = pPlugins;
    while (pPlugin)
    {
        fMQWriteBC WriteBC = (fMQWriteBC)GetProcAddress(pPlugin->hModule, "OnWriteBC");
        if (WriteBC)
        {
            if (!WriteBC(szText)) bWrite = false;
        }
        pPlugin = pPlugin->pNext;
    }
    if (!bWrite) return;

    if (SET->Window)
    {
        WINDOW->Write(szText);
        return;
    }
    WriteChatColor(szText);
}

void CEQBCWnd::ResetKeybinds()
{
    WINDOW->ResetKeys();
}

// -------------------------------------------------------------
// connection management class (handles majority of plugin i/o)

class CConnectionMgr
{
public:
    bool Connected;
    bool Connecting;
    bool TriedConnect;
    unsigned long LastSecs;

    void NewThread()
    {
        InitializeCriticalSection(&ConnectCS);
    };

    void KillThread()
    {
        EnterCriticalSection(&ConnectCS);
        LeaveCriticalSection(&ConnectCS);
        DeleteCriticalSection(&ConnectCS);
    };

    void Transmit(bool bHandleDisconnect, char* szMsg)
    {
        if (!Connected) return;
        int iErr = 0;
        iErr = send(theSocket, szMsg, strlen(szMsg), 0);
        if (bHandleDisconnect) CheckError("Transmit:SendMsg", iErr);
        iErr = send(theSocket, SEND_LINE_TERM, strlen(SEND_LINE_TERM), 0);
        if (bHandleDisconnect) CheckError("Transmit:SendTerm", iErr);
    };

    void SendLocalEcho()
    {
        if (!Connected) return;
        int  iErr          = 0;
        char szCommand[15] = {0};
        sprintf(szCommand, "%s%i\n", CMD_LOCALECHO, SET->LocalEcho);
        iErr = send(theSocket, szCommand, strlen(szCommand), 0);
        CheckError("SendLocalEcho:Send1", iErr);
    };

    void BC(char* szLine)
    {
        if (!ConnectReady()) return;
        if (szLine && strlen(szLine))
        {
            Transmit(true, szLine);
        }
    };

    void BCT(char* szLine)
    {
        if (!ConnectReady()) return;
        char szCmdBct[] = CMD_TELL;
        if (szLine && strlen(szLine))
        {
            ChanTransmit(szCmdBct, szLine);
            if (SET->IRCMode && !SET->SilentOutMsg)
            {
                char szTemp[MAX_STRING] = {0};
                int iSrc                = 0;
                int iDest               = 0;
                int iLen                = 0;

                iLen   = strlen(szLine);
                iDest += WriteStringGetCount(&szTemp[iDest], COLOR_STELL1);
                while (szLine[iSrc] != ' ' && szLine[iSrc] != '\n' && iSrc <= iLen)
                {
                    szTemp[iDest++] = szLine[iSrc++];
                }
                iDest += WriteStringGetCount(&szTemp[iDest], COLOR_STELL2);
                iSrc++;
                while (iSrc <= iLen)
                {
                    szTemp[iDest++] = szLine[iSrc++];
                }
                szTemp[iDest] = '\n';
                WriteOut(szTemp);
            }
        }
    };

    bool BCA(char* szLine)
    {
        if (!ConnectReady()) return false;

        char szCmdAll[] = CMD_MSGALL;
        if (szLine && strlen(szLine))
        {
            if (SET->EchoAll)
            {
                char szTemp[MAX_STRING] = {0};
                sprintf(szTemp, "<%s> [+r+]([+o+]to all[+r+])[+w+] %s", ((PSPAWNINFO)pLocalPlayer)->Name, szLine);
                HandleIncomingString(szTemp, false);
            }
            ChanTransmit(szCmdAll, szLine);
        }
        return true;
    };

    void BCAA(PSPAWNINFO pLPlayer, char* szLine)
    {
        if (!BCA(szLine)) return;

        char szTemp[MAX_STRING] = {0};
        sprintf(szTemp, "<%s> %s %s", pLPlayer->Name, pLPlayer->Name, szLine);
        HandleIncomingString(szTemp, true);
    };

    void HandleChannels(char* szLine)
    {
        char  szTemp1[MAX_STRING]  = {0};
        char  szTemp2[MAX_STRING]  = {0};
        char* szArg                = NULL;
        char  szCmdChan[]          = CMD_CHANNELS;

        if (!ConnectReady()) return;

        char* pName = GetCharName();

        if (!szLine)
        {
            GetPrivateProfileString(pName ? pName : "Settings", "Channels", "", szTemp2, MAX_STRING, INIFileName);
            strlwr(szTemp2);
            if (!(szArg = strtok(szTemp2, " \n"))) return;
        }
        else
        {
            strlwr(szLine);
            szArg = strtok(szLine, " \n"); // first token will be command CHANNELS, skip it
            szArg = strtok(NULL,   " \n");
        }

        while (szArg != NULL)
        {
            strncat(szTemp1, szArg, MAX_STRING - 1);
            if ((szArg = strtok(NULL, " \n"))) strcat(szTemp1, " ");
        }

        if (*pName) WritePrivateProfileString(pName, "Channels", szTemp1, INIFileName);

        ChanTransmit(szCmdChan, szTemp1);
    };

    void ConnectINI(char* szName)
    {
        if (Connecting)
        {
            WriteOut("\ar#\ax Already trying to connect! Hold on a minute there...");
            return;
        }
        if (!*szName)
        {
            WriteOut("\ar#\ax No INI key name given. Aborting...");
            return;
        }
        if (Connected)
        {
            Disconnect(true);
        }

        char szMsg[MAX_STRING]    = {0};
        char szTemp[MAX_STRING]   = {0};
        char szPass[MAX_PASSWORD] = {0};

        SetPlayer();

        GetPrivateProfileString(szName, "Server",   NULL,   szTemp, MAX_STRING, INIFileName);
        if (!*szTemp)
        {
            sprintf(szMsg, "\ar#\ax Server for key \ay%s\ax not found.", szName);
            WriteOut(szMsg);
            return;
        }
        strcpy(szServer,   szTemp);
        GetPrivateProfileString(szName, "Port",     "2112", szTemp, MAX_STRING, INIFileName);
        strcpy(szPort,     szTemp);
        GetPrivateProfileString(szName, "Password", "",     szPass, MAX_STRING, INIFileName);
        strcpy(szPassword, szPass);
        DoConnectEQBCS();
    };

    void Connect(char* szLine, bool bForce)
    {
        if (Connected && !bForce)
        {
            WriteOut("\ar#\ax Already connected. Use \ag/bccmd quit\ax to disconnect first.");
            return;
        }
        if (Connecting)
        {
            WriteOut("\ar#\ax Already trying to connect! Hold on a minute there...");
            return;
        }
        if (Connected && bForce)
        {
            Disconnect(true);
        }

        char szCurArg[MAX_STRING] = {0};
        char szTemp[MAX_STRING]   = {0};
        char szPass[MAX_PASSWORD] = {0};

        SetPlayer();

        GetArg(szCurArg, szLine, 2); // 1 was the connect statement.
        if (!*szCurArg)
        {
            GetPrivateProfileString("Last Connect", "Server",   "127.0.0.1", szTemp, MAX_STRING, INIFileName);
            strcpy(szServer,   szTemp);
            GetPrivateProfileString("Last Connect", "Port",     "2112",      szTemp, MAX_STRING, INIFileName);
            strcpy(szPort,     szTemp);
            GetPrivateProfileString("Last Connect", "Password", "",          szPass, MAX_STRING, INIFileName);
            strcpy(szPassword, szPass);
        }
        else
        {
            strcpy(szServer, szCurArg);
            GetArg(szCurArg, szLine, 3);
            if (!*szCurArg)
            {
                GetPrivateProfileString("Last Connect", "Port",     "2112",  szTemp, MAX_STRING, INIFileName);
                strcpy(szPort,     szTemp);
                GetPrivateProfileString("Last Connect", "Password", "",      szPass, MAX_STRING, INIFileName);
                strcpy(szPassword, szPass);
            }
            else
            {
                strcpy(szPort, szCurArg);
                GetArg(szCurArg, szLine, 4);
                if (!*szCurArg)
                {
                    GetPrivateProfileString("Last Connect", "Password", "",  szPass, MAX_STRING, INIFileName);
                    strcpy(szPassword, szPass);
                }
                else
                {
                    strcpy(szPassword, szCurArg);
                }
            }
        }
        DoConnectEQBCS();
        return;
    };

    void HandleControlMsg(char* pRawmsg)
    {
        if (!strncmp(pRawmsg, "PING", 4))
        {
            LastPing = clock();
            Transmit(true, CMD_PONG);
        }
        else if (!strncmp(pRawmsg, "NBPKT:", 6))
        {
            SendNetBotMsg(pRawmsg + 6);
        }
        else if (!strncmp(pRawmsg, "NB", 2))
        {
            //char Output[64] = {0}; //long interface name overflow
            char szOutput[MAX_STRING] = {0};
            if (!strncmp(pRawmsg, "NBJOIN=", 7))
            {
                sprintf(szOutput, "\ar#\ax - %s has joined the server.", &pRawmsg[7]);
                WriteOut(szOutput);
            }
            if (!strncmp(pRawmsg, "NBQUIT=", 7))
            {
                sprintf(szOutput, "\ar#\ax - %s has left the server.", &pRawmsg[7]);
                WriteOut(szOutput);
            }
            SendNetBotEvent(pRawmsg);
        }
    };

    void HandleIncomingString(char* pRawmsg, bool bForce)
    {
        if (!pRawmsg) return;
        char szTemp[MAX_STRING] = {0};
        char cLastChar          = 0;
        int  iSrc               = 0;
        int  iDest              = 0;
        int  iCharCount         = -1;
        int  iMsgTell           = 0;
        char cColor             = 0;
        unsigned char bSysMsg  = FALSE;
        char* pszCmdStart       = NULL;
        bool  bBciCmd           = false;

        bSysMsg = (*pRawmsg == '-') ? TRUE : FALSE;

        if (*pRawmsg == '\t')
        {
            HandleControlMsg(pRawmsg + 1);
            return;
        }

        while (pRawmsg[iSrc] != 0 && iDest < MAX_STRING - 1)
        {
            if (iSrc == 0 && bSysMsg) // first char
            {
                iDest      += WriteStringGetCount(&szTemp[iDest], "\ar#\ax ");
                cLastChar   = *pRawmsg;
                iCharCount  = 1;
            }
            else if (cLastChar != ' ' || pRawmsg[iSrc] != ' ') // Do not add extra spaces
            {
                if (iCharCount <= 1 && strnicmp(&pRawmsg[iSrc], szToonCmdStart, strlen(szToonCmdStart)) == 0)
                {
                    pszCmdStart = &pRawmsg[iSrc + strlen(szToonCmdStart) - 1];
                }
                if (iCharCount <= 1 && iMsgTell == 1 && strnicmp(&pRawmsg[iSrc], "//", 2) == 0)
                {
                    pszCmdStart = &pRawmsg[iSrc + 1];
                }
                if (iCharCount <= 1 && bBciCmd)
                {
                    pszCmdStart = &pRawmsg[iSrc];
                }
                if (iCharCount >= 0)
                {
                    iCharCount++;
                    // if not in cmdMode and have room, check for color code.
                    if (pszCmdStart == NULL && (cColor = GetCharColor(&pRawmsg[iSrc])) != 0 && (iDest + (isupper(cColor) ? 5 : 4)) < MAX_STRING)
                    {
                        //DebugSpewAlways("Got Color: %c", cColor);
                        unsigned char bDark        = isupper(cColor);
                        szTemp[iDest++]            = '\a';
                        if (bDark) szTemp[iDest++] = '-';
                        szTemp[iDest++]            = tolower(cColor);
                        cLastChar                  = '\a'; // Something that should not exist otherwise
                        iSrc                      += 4; // Color code format is [+x+] - 5 characters.
                    }
                    else
                    {
                        cLastChar = szTemp[iDest] = pRawmsg[iSrc];
                        iDest++;
                    }
                }
                else if (!bSysMsg)
                {
                    switch (pRawmsg[iSrc])
                    {
                    case '<':
                        {
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME_BRACKET);
                            szTemp[iDest++] = SET->IRCMode ? '<' : '>';
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME);
                            break;
                        }
                    case '>':
                        {
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME_BRACKET);
                            szTemp[iDest++] = SET->IRCMode ? '>' : '<';
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iCharCount      = 0;
                            break;
                        }
                    case '[':
                        {
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME_BRACKET);
                            szTemp[iDest++] = '[';
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME);
                            break;
                        }
                    case ']':
                        {
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME_BRACKET);
                            if (SET->IRCMode)
                            {
                                szTemp[iDest++] = '(';
                                iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                                iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME);
                                iDest          += WriteStringGetCount(&szTemp[iDest], "msg");
                                iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                                iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_NAME_BRACKET);
                                iDest          += WriteStringGetCount(&szTemp[iDest], ")]");
                            }
                            else
                            {
                                szTemp[iDest++] = ']';
                            }
                            iDest          += WriteStringGetCount(&szTemp[iDest], COLOR_OFF);
                            iCharCount      = 0;
                            iMsgTell        = 1;
                            break;
                        }
                    case '{':
                        break;
                    case '}':
                        iDest++;
                        iCharCount = 0;
                        bBciCmd    = true;
                        break;
                    default:
                        {
                            cLastChar = szTemp[iDest] = pRawmsg[iSrc];
                            iDest++;
                            break;
                        }
                    }
                }
            }
            iSrc++;
        }

        if (pszCmdStart)
        {
            if (bBciCmd)
            {
                HandleBciMsg(szTemp, pszCmdStart);
                return;
            }
            HandleIncomingCmd(pszCmdStart, bForce);
            if (SET->SilentIncCmd) return;
        }
        WriteOut(szTemp);
    };

    void Disconnect(bool bSend)
    {
        if (Connected)
        {
            SendNetBotEvent("NBEXIT");
            Connected = false;
            // Could set linger off here..
            if (bSend)
            {
                int iErr = send(theSocket, CMD_DISCONNECT, sizeof(CMD_DISCONNECT), 0);
                CheckError("Disconnect:Send", iErr);
            }
            closesocket(theSocket);
            LastReadPos = 0;
        }
    };

    void Pulse()
    {
        if (TriedConnect) ConnectStatus();

        if (Connected)
        {
            HandleBuffer();
        }
        else if (LastSecs > 0 && !Connecting)
        {
            if (LastSecs + SET->ReconnectSecs < GetTickCount() / 1000)
            {
                LastSecs = GetTickCount() / 1000;
                Connect("", false);
            }
        }
    };

    void Status()
    {
        char szTemp[MAX_STRING] = {0};
        if (Connected)
        {
            sprintf(szTemp,"\ar#\ax MQ2Eqbc Status: ONLINE - %s - %s", szServer, szPort);
            WriteOut(szTemp);
        }
        else
        {
            WriteOut("\ar#\ax MQ2Eqbc Status: OFFLINE");
        }
        sprintf(szTemp,"\ar#\ax Allow Control: %s, Auto Connect: %s", SET->AllowControl ? "ON" : "OFF", SET->AutoConnect ? "ON" : "OFF");
        WriteOut(szTemp);
        sprintf(szTemp,"\ar#\ax IRC Compat Mode: %s, Reconnect: %s: (every %d secs)", SET->IRCMode ? "ON" : "OFF", SET->AutoReconnect ? "ON" : "OFF", SET->ReconnectSecs);
        WriteOut(szTemp);
    };

    void Reconnect()
    {
        Disconnect(true);
        Connect("", false);
    };

    void Version()
    {
        char szTemp[MAX_STRING] = {0};
        sprintf(szTemp, "\ar#\ax %s", PROG_VERSION);
        WriteOut(szTemp);
    };

    void Names()
    {
        if (!ConnectReady()) return;
        WriteOut("\ar#\ax Requesting names...");
        int iErr = send(theSocket, CMD_NAMES, strlen(CMD_NAMES), 0);
        CheckError("HandleNamesRequest:send", iErr);
    };

    void AutoConnect()
    {
        if (!SET->AutoConnect || Connected) return;
        SetPlayer();
        Connect("", false);
    };

    void Logout()
    {
        if (!Connected) return;
        Disconnect(true);
    };

    CConnectionMgr()
    {
        Connected     = false;
        Connecting    = false;
        TriedConnect  = false;
        SocketGone    = 0;
        LastReadPos   = 0;
        LastSecs      = 0;
        pcReadBuf     = new char[MAX_READBUF];
        LastPing      = 0;
        usSockVersion = 0;
    };

private:
    bool ConnectReady()
    {
        if (!Connected)
        {
            WriteOut("\ar#\ax You are not connected. Use \ag/bccmd connect\ax to establish a connection.");
            return false;
        }
        return true;
    };

    void CheckSocket(char* szFunc, int iErr)
    {
        int iWerr = WSAGetLastError();
        if (iWerr == WSAECONNABORTED)
        {
            SocketGone = TRUE;
        }
        WSASetLastError(0);
        // DebugSpewAlways("Sock Error-%s: %d / w%d", szFunc, err, werr);
    };

    void CheckError(char* szFunc, int iErr)
    {
        if (iErr == SOCKET_ERROR || (iErr == 0 && WSAGetLastError() != WSAEWOULDBLOCK))
        {
            CheckSocket(szFunc, iErr);
        }
    };

    void ConnectStatus()
    {
        TriedConnect = false;
        if (Connected)
        {
            WriteOut("\ar#\ax Connected!");
            SET->UpdateServer();
            LastReadPos = 0;
            LastSecs    = 0;
            HandleChannels(NULL);
            SendLocalEcho();
            return;
        }
        WriteOut("\ar#\ax Could not connect.");
    };

    void DoConnectEQBCS()
    {
        char szMsg[MAX_STRING]    = {0};

        usSockVersion = MAKEWORD(1, 1);
        WSAStartup(usSockVersion, &wsaData);
        pHostEntry = gethostbyname(szServer);
        if (!pHostEntry)
        {
            WriteOut("\ar#\ax gethostbyname error");
            WSACleanup();
            return;
        }
        theSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (theSocket == INVALID_SOCKET)
        {
            WriteOut("\ar#\ax Socket error");
            WSACleanup();
            return;
        }

        // custom UI window update
        WINDOW->Create();
        WINDOW->UpdateTitle();

        sprintf(szMsg, "\ar#\ax Connecting to %s %s...", szServer, szPort);
        WriteOut(szMsg);

        LastPing               = 0;
        serverInfo.sin_family  = AF_INET;
        serverInfo.sin_addr    = *((LPIN_ADDR)*pHostEntry->h_addr_list);
        serverInfo.sin_port    = htons(atoi(szPort));
        unsigned long ThreadId = 0;
        CreateThread(NULL, 0, &EQBCConnectThread, 0, 0, &ThreadId);
    };

    void HandleBuffer()
    {
        int iErr = 0;
        // Fill the input buffer with new data, if any
        for ( ; LastReadPos < (MAX_READBUF - 1) ; LastReadPos++)
        {
            iErr = recv(theSocket, &pcReadBuf[LastReadPos], 1, 0);
            if ((pcReadBuf[LastReadPos] == '\n') || (iErr == 0) || (iErr == SOCKET_ERROR))
            {
                if (pcReadBuf[LastReadPos] == '\n')
                {
                    pcReadBuf[LastReadPos] = '\0';
                    HandleIncomingString(pcReadBuf, false);
                    LastReadPos = -1;
                }
                //break;
            }
            if (iErr == 0 || iErr == SOCKET_ERROR) break;
        }

        if (LastReadPos < 0) LastReadPos = 0;
        if (iErr == 0 && WSAGetLastError() == 0)
        {
            // Should be giving WSAWOULDBLOCK
            SocketGone = TRUE;
        }

        if (SocketGone)
        {
            SocketGone = FALSE;
            HandleIncomingString("-- Remote connection closed, you are no longer connected", false);
            Disconnect(false);
            if (SET->AutoReconnect && SET->ReconnectSecs > 0)
            {
                LastSecs = GetTickCount() / 1000;
            }
        }
        if (LastPing > 0 && LastPing + 120000 < clock())
        {
            WriteOut("\arMQ2EQBC: did not recieve expected ping from server, pinging...");
            Transmit(true, CMD_PONG);
            LastPing = 0;
        }
    };

    void HandleIncomingCmd(char* pszCmd, bool bForce)
    {
        char  szTemp[MAX_STRING] = {0};
        if (!SET->AllowControl && !bForce)
        {
            if (!SET->SilentCmd)
            {
                sprintf(szTemp, "\ar#\ax CMD: [%s] - Not allowed (Control turned off)", pszCmd);
                WriteOut(szTemp);
            }
            if (SET->NotifyControl)
            {
                sprintf(szTemp, "Command [ %s ] not processed (Control turned off)", pszCmd);
                Transmit(true, szTemp);
            }
            return;
        }
        if (!SET->SilentCmd)
        {
            sprintf(szTemp, "\ar#\ax CMD: [%s]", pszCmd);
            WriteOut(szTemp);
        }

        if (!pLocalPlayer) return;
        sprintf(szTemp, pszCmd);
        CleanEnd(szTemp);
        DoCommand((PSPAWNINFO)pLocalPlayer, szTemp);
    };

    void HandleBciMsg(char* szName, char* szMsg)
    {
        // EQBC Interface handling
        char szBuff[1024] = {0};
        if (!strncmp(szMsg, "REQ", 3))
        {
            if (GetGameState() == GAMESTATE_INGAME && GetCharInfo()->pSpawn)
            {
                sprintf(szBuff, "1|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%d|%d|%d|%c|%c", GetCurHPS(), GetMaxHPS(), GetCharInfo2()->Mana, GetMaxMana(), GetCharInfo2()->Endurance, GetMaxEndurance(),
                    GetCharInfo()->zoneId, GetCharInfo2()->Level, GetClassDesc(GetCharInfo2()->Class), pEverQuest->GetRaceDesc(GetCharInfo2()->Race), GetCharInfo()->Exp,
                    GetCharInfo()->AAExp, GetCharInfo2()->AAPoints, ((PSPAWNINFO)pCharSpawn)->StandState, (((PSPAWNINFO)pCharSpawn)->PetID == 0xFFFFFFFF) ? '0' : '1');
                BciTransmit(szName, szBuff);
            }
        }
    };

    void ChanTransmit(char* szCommand, char* szLine)
    {
        int iErr = 0;
        iErr = send(theSocket, szCommand,      strlen(szCommand),      0);
        CheckError("ChanTransmit:SendCmd",  iErr);
        iErr = send(theSocket, szLine,         strlen(szLine),         0);
        CheckError("ChanTransmit:SendLine", iErr);
        iErr = send(theSocket, SEND_LINE_TERM, strlen(SEND_LINE_TERM), 0);
        CheckError("ChanTransmit:SendTerm", iErr);
    };

    void BciTransmit(char* szLine, char* szCmd)
    {
        char szCommand[] = CMD_BCI;
        int  iErr        = 0;

        strcat(szLine, " ");
        strcat(szLine, szCmd);

        if (szLine && strlen(szLine))
        {
            iErr = send(theSocket, szCommand,      strlen(szCommand),      0);
            CheckError("BciTransmit:Send1", iErr);
            iErr = send(theSocket, szLine,         strlen(szLine),         0);
            CheckError("BciTransmit:Send2", iErr);
            iErr = send(theSocket, SEND_LINE_TERM, strlen(SEND_LINE_TERM), 0);
            CheckError("BciTransmit:Send3", iErr);
        }
    };

    void SendNetBotMsg(char* szMess)
    {
        char* pPosi = NULL;
        char* pName = NULL;
        if (szMess != NULL && *szMess != NULL)
        {
            if (pPosi = strchr(szMess, ':'))
            {
                if (!strnicmp(pPosi, ":[NB]|", 6))
                {
                    pName = "mq2netbots";
                }
                else if (!strnicmp(pPosi, ":[NH]|", 6))
                {
                    pName = "mq2netheal";
                }
            }
        }
        if (pName == NULL || *pName == NULL)
        {
            DebugSpew("SendNetBotMSG::Bad NBMSG");
            return;
        }

        fNetBotOnMsg pfSendf = NULL;
        PMQPLUGIN    pFind   = pPlugins;
        while (pFind && stricmp(pFind->szFilename, pName))
        {
            pFind = pFind->pNext;
        }
        if (pFind)
        {
            pfSendf = (fNetBotOnMsg)GetProcAddress(pFind->hModule, "OnNetBotMSG");
        }
        if (pfSendf == NULL)
        {
            DebugSpew("SendNetBotMSG::No Handler");
            return;
        }

        *pPosi = 0;
        pfSendf(szMess, pPosi + 1);
        *pPosi = ':';
    };

    void SendNetBotEvent(char* szMess)
    {
        if (szMess == NULL || *szMess == NULL)
        {
            DebugSpew("SendNetBotEVENT::Bad NBMSG");
            return;
        }

        fNetBotOnEvent pfSendf = NULL;
        PMQPLUGIN      pFind   = pPlugins;
        while (pFind && stricmp(pFind->szFilename, "mq2netbots"))
        {
            pFind = pFind->pNext;
        }
        if (pFind)
        {
            pfSendf = (fNetBotOnEvent)GetProcAddress(pFind->hModule, "OnNetBotEVENT");
        }
        if (pfSendf == NULL)
        {
            DebugSpew("SendNetBotEVENT::No Handler");
            return;
        }
        pfSendf(szMess);
    };

    int WriteStringGetCount(char* pDest, char* pSrc)
    {
        int i = 0;
        for( ; pDest && pSrc && *pSrc; pSrc++)
        {
            pDest[i++] = *pSrc;
        }
        return i;
    };

    char GetCharColor(char* pTest)
    {
        // Colors From MQToSTML (already assigned here to szColorChars)
        // 'y'ellow, 'o'range, 'g'reen, bl'u'e, 'r'ed, 't'eal, 'b'lack (none),
        // 'm'agenta, 'p'urple, 'w'hite, 'x'=back to default
        // Color code format: "[+r+]"
        if ( pTest[0] == '['  &&
             pTest[1] == '+'  &&
             pTest[2] != '\0' &&
             pTest[3] == '+'  &&
             pTest[4] == ']')
        {
            if (strchr(szColorChars, (int)tolower(pTest[2])))
            {
                return pTest[2];
            }
        }
        return 0;
    };

    void CleanEnd(char* pszStr)
    {
        // Remove trailing spaces and CR/LF's
        if (pszStr && *pszStr)
        {
            int iLen = 0;
            for (iLen = strlen(pszStr) - 1; iLen >= 0 && strchr(" \r\n", pszStr[iLen]) ; pszStr[iLen--] = 0);
        }
    };

    int                SocketGone;
    int                LastReadPos;
    char*              pcReadBuf;
    clock_t            LastPing;
    WSADATA            wsaData;
    hostent*           pHostEntry;
    unsigned short     usSockVersion;
};

unsigned long __stdcall EQBCConnectThread(void* lpParam)
{
    EnterCriticalSection(&ConnectCS);
    EQBC->Connecting = true;
    iNRet = connect(theSocket, (sockaddr*)&serverInfo, sizeof(struct sockaddr));

    if (iNRet == SOCKET_ERROR)
    {
        EQBC->Connected = false;
    }
    else
    {
        unsigned long ulNonblocking = 1;
        ioctlsocket(theSocket, FIONBIO, &ulNonblocking);
        Sleep((clock_t)4 * CLOCKS_PER_SEC/2);

        send(theSocket, CONNECT_START, strlen(CONNECT_START), 0);
        if (*szPassword)
        {
            // DebugSpew("With Password");
            send(theSocket, CONNECT_PWSEP, strlen(CONNECT_PWSEP), 0);
            send(theSocket,    szPassword,    strlen(szPassword), 0);
        }
        send(theSocket, CONNECT_START2, strlen(CONNECT_START2), 0);
        send(theSocket,     szToonName,     strlen(szToonName), 0);
        send(theSocket,    CONNECT_END,    strlen(CONNECT_END), 0);
        EQBC->Connected = true;
    }

    EQBC->TriedConnect = true;
    EQBC->Connecting   = false;
    LeaveCriticalSection(&ConnectCS);
    return 0;
}

// --------------------------------------
// placement
void CSettingsMgr::HandleWnd(bool bCreate)
{
    if (bCreate)
    {
        WINDOW->Create();
        return;
    }
    WINDOW->Destroy(true);
}

void CSettingsMgr::HandleEcho()
{
    EQBC->SendLocalEcho();
}

void CEQBCWnd::WriteToBC(char* szBuffer)
{
    EQBC->BC(szBuffer);
}

// --------------------------------------
// Custom ${EQBC} TLO
class EQBCType : public MQ2Type
{
public:
    enum VarMembers
    {
        Connected = 1,
        Server    = 2,
        Port      = 3,
        ToonName  = 4,
        Setting   = 5,
    };
    EQBCType();
    bool GetMember(MQ2VARPTR VarPtr,   char* Member, char* Index, MQ2TYPEVAR &Dest);
    bool ToString(MQ2VARPTR VarPtr,    char* Destination);
    bool FromData(MQ2VARPTR &VarPtr,   MQ2TYPEVAR &Source);
    bool FromString(MQ2VARPTR &VarPtr, char* Source);
    bool OptStatus(char* Index);
};

EQBCType::EQBCType():MQ2Type("EQBC")
{
    TypeMember(Connected);
    TypeMember(Server);
    TypeMember(Port);
    TypeMember(ToonName);
    TypeMember(Setting);
}

bool EQBCType::OptStatus(char* Index)
{
    int* iOption = NULL;
    if (!stricmp(Index, szSetAutoConnect))
    {
        iOption = &SET->AutoConnect;
    }
    else if (!stricmp(Index, szSetControl))
    {
        iOption = &SET->AllowControl;
    }
    else if (!stricmp(Index, szSetCompatMode))
    {
        iOption = &SET->IRCMode;
    }
    else if (!stricmp(Index, szSetAutoReconnect))
    {
        iOption = &SET->AutoReconnect;
    }
    else if (!stricmp(Index, szSetWindow))
    {
        iOption = &SET->Window;
    }
    else if (!stricmp(Index, szSetEchoAll))
    {
        iOption = &SET->EchoAll;
    }
    else if (!stricmp(Index, szSetLocalEcho))
    {
        iOption = &SET->LocalEcho;
    }
    else if (!stricmp(Index, szSetTellWatch))
    {
        iOption = &SET->WatchTell;
    }
    else if (!stricmp(Index, szSetGuildWatch))
    {
        iOption = &SET->WatchGuild;
    }
    else if (!stricmp(Index, szSetGroupWatch))
    {
        iOption = &SET->WatchGroup;
    }
    else if (!stricmp(Index, szSetFSWatch))
    {
        iOption = &SET->WatchFsay;
    }
    else if (!stricmp(Index, szSetSilentCmd))
    {
        iOption = &SET->SilentCmd;
    }
    else if (!stricmp(Index, szSetSaveByChar))
    {
        iOption = &SET->SaveByChar;
    }
    else if (!stricmp(Index, szSetSilentIncCmd))
    {
        iOption = &SET->SilentIncCmd;
    }
    else if (!stricmp(Index, szSetSilentOutMsg))
    {
        iOption = &SET->SilentOutMsg;
    }
    else if (!stricmp(Index, szSetNotifyControl))
    {
        iOption = &SET->NotifyControl;
    }
    return (!iOption ? false : (*iOption ? true : false));
}

bool EQBCType::GetMember(MQ2VARPTR VarPtr, char* Member, char* Index, MQ2TYPEVAR &Dest)
{
    PMQ2TYPEMEMBER pMember=EQBCType::FindMember(Member);
    if (!pMember || !EQBC) return false;
    switch ((VarMembers)pMember->ID)
    {
        case Connected:
            Dest.DWord = EQBC->Connected;
            Dest.Type  = pBoolType;
            return true;
        case Server:
            sprintf(DataTypeTemp, "OFFLINE");
            if (EQBC->Connected)
            {
                strcpy(DataTypeTemp, szServer);
            }
            Dest.Ptr  = DataTypeTemp;
            Dest.Type = pStringType;
            return true;
        case Port:
            sprintf(DataTypeTemp, "OFFLINE");
            if (EQBC->Connected)
            {
                strcpy(DataTypeTemp, szPort);
            }
            Dest.Ptr  = DataTypeTemp;
            Dest.Type = pStringType;
            return true;
        case ToonName:
            sprintf(DataTypeTemp, "OFFLINE");
            if (EQBC->Connected)
            {
                strcpy(DataTypeTemp, szToonName);
            }
            Dest.Ptr  = DataTypeTemp;
            Dest.Type = pStringType;
            return true;
        case Setting:
            Dest.DWord = false;
            Dest.Type  = pBoolType;
            if (!Index[0]) return true;
            Dest.DWord = OptStatus(Index);
            return true;
    }
    return false;
}

bool EQBCType::ToString(MQ2VARPTR VarPtr, char* Destination)
{
    strcpy(Destination,"EQBC");
    return true;
}

bool EQBCType::FromData(MQ2VARPTR &VarPtr, MQ2TYPEVAR &Source)
{
    return false;
}

bool EQBCType::FromString(MQ2VARPTR &VarPtr, char* Source)
{
    return false;
}

int dataEQBC(char* Index, MQ2TYPEVAR &Dest)
{
    Dest.DWord = 1;
    Dest.Type  = pEQBCType;
    return true;
}

// --------------------------------------
// Command input

void OutputHelp()
{
    EQBC->Status();
    WriteOut("\ar#\ax Commands Available");
    WriteOut("\ar#\ax \ay/bc your text\ax (send text)");
    WriteOut("\ar#\ax \ay/bc ToonName //command\ax (send Command to ToonName - \arDEPRECATED, use /bct)");
    WriteOut("\ar#\ax \ay/bct ToonName your text\ax (send your text to specific Toon)");
    WriteOut("\ar#\ax \ay/bct ToonName //command\ax (send Command to ToonName)");
    WriteOut("\ar#\ax \ay/bca //command\ax (send Command to all connected names EXCLUDING yourself)");
    WriteOut("\ar#\ax \ay/bcaa //command\ax (send Command to all connected names INCLUDING yourself)");
    WriteOut("\ar#\ax \ay/bccmd connect <server> <port> <pw>\ax (defaults: 127.0.0.1 2112)");
    WriteOut("\ar#\ax \ay/bccmd forceconnect <server> <port> <pw>\ax (defaults: 127.0.0.1 2112)");
    WriteOut("\ar#\ax \ay/bccmd iniconnect [INIKeyName]");
    WriteOut("\ar#\ax \ay/bccmd quit\ax (disconnects)");
    WriteOut("\ar#\ax \ay/bccmd help\ax (show this information)");
    WriteOut("\ar#\ax \ay/bccmd status\ax (show connected or not and settings)");
    WriteOut("\ar#\ax \ay/bccmd reconnect\ax (close current connection and connect again)");
    WriteOut("\ar#\ax \ay/bccmd names\ax (show who is connected)");
    WriteOut("\ar#\ax \ay/bccmd version\ax (show plugin version)");
    WriteOut("\ar#\ax \ay/bccmd colordump\ax (Shows all available color codes)");
    WriteOut("\ar#\ax \ay/bccmd channels <channel list>\ax (set list of channels to receive tells from)");
    WriteOut("\ar#\ax \ay/bccmd stopreconnect\ax (stop trying to reconnect for now)");
    WriteOut("\ar#\ax Settings Available");
    WriteOut("\ar#\ax \ay/bccmd set reconnectsecs n\ax (n is seconds to reconnect: default 15)");
    WriteOut("\ar#\ax \ay/bccmd set OPTION\ax [ \ayon\ax | \ayoff\ax ] (set OPTION directly)");
    WriteOut("\ar#\ax \ay/bccmd toggle OPTION\ax (toggles OPTION) - where OPTION may be:");
    WriteOut("\aw -- \ayautoconnect\ax (auto connect to previous server)");
    WriteOut("\aw -- \aycontrol\ax (allow remote control)");
    WriteOut("\aw -- \aycompatmode\ax (IRC Compatability mode)");
    WriteOut("\aw -- \ayreconnect\ax (auto-reconnect mode on server disconnect)");
    WriteOut("\aw -- \aywindow\ax (use dedicated chat window)");
    WriteOut("\aw -- \aylocalecho\ax (echo my commands back to me if I am in channel)");
    WriteOut("\aw -- \aytellwatch\ax (relay tells received to /bc)");
    WriteOut("\aw -- \ayguildwatch\ax (relay guild chat to /bc)");
    WriteOut("\aw -- \aygroupwatch\ax (relay group chat to /bc)");
    WriteOut("\aw -- \ayfswatch\ax (relay fellowship chat to /bc)");
    WriteOut("\aw -- \aysilentcmd\ax (display 'CMD: [command]' echo)");
    WriteOut("\aw -- \aysavebychar\ax (save UI data to [CharName] INI section)");
    WriteOut("\aw -- \aysilentinccmd\ax (display incoming commands)");
    WriteOut("\aw -- \aynotifycontrol\ax (notify /bc when disabled control blocks incoming command)");
    WriteOut("\aw -- \aysilentoutmsg\ax (squelch outgoing msgs regardless of compatmode enabled)");
    WriteOut("\aw -- \ayechoall\ax (echo outgoing /bca messages)");
}

// bccmd
void BccmdCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    char szArg[MAX_STRING] = {0};
    char szMsg[MAX_STRING] = {0};

    GetArg(szArg, szLine, 1);

    if (!strnicmp(szArg, "set", 4))
    {
        char szTempSet[MAX_STRING] = {0};
        sprintf(szTempSet, "%s", GetNextArg(szLine, 1, FALSE, 0));
        SET->Change(szTempSet, false);
        return;
    }
    else if (!strnicmp(szArg, "toggle", 7))
    {
        char szTempSet[MAX_STRING] = {0};
        sprintf(szTempSet, "%s", GetNextArg(szLine, 1, FALSE, 0));
        SET->Change(szTempSet, true);
        return;
    }

    if (!strnicmp(szArg, szCmdHelp, sizeof(szCmdHelp)))
    {
        OutputHelp();
    }
    else if (!strnicmp(szArg, szCmdConnect, sizeof(szCmdConnect)))
    {
        EQBC->Connect(szLine, false);
    }
    else if (!strnicmp(szArg, szCmdDisconnect, sizeof(szCmdDisconnect)))
    {
        EQBC->Disconnect(true);
        WriteOut("\ar#\ax Connection Closed, you can unload MQ2Eqbc now.");
    }
    else if (!strnicmp(szArg, szCmdStatus, sizeof(szCmdStatus)))
    {
        EQBC->Status();
    }
    else if (!strnicmp(szArg, szCmdReconnect, sizeof(szCmdReconnect)))
    {
        EQBC->Reconnect();
    }
    else if (!strnicmp(szArg, szCmdNames, sizeof(szCmdNames)))
    {
        EQBC->Names();
    }
    else if (!strnicmp(szArg, szCmdRelog, sizeof(szCmdRelog)))
    {
        WriteOut("\ar#\ax Relog command removed. Try out MQ2SwitchChar by ieatacid.");
    }
    else if (!strnicmp(szArg, szCmdVersion, sizeof(szCmdVersion)))
    {
        sprintf(szMsg, "\ar#\ax %s", PROG_VERSION);
        WriteOut(szMsg);
    }
    else if (!strnicmp(szArg, szCmdColorDump, sizeof(szCmdColorDump)))
    {
        char cCh = 0;
        int  i   = 0;
        strcpy(szMsg, "\ar#\ax Bright Colors:");
        for (i = 0; i < (int)strlen(szColorChars) - 1; i++)
        {
            cCh = szColorChars[i];
            sprintf(szArg, " \a%c[+%c+]", cCh, cCh);
            strcat(szMsg, szArg);
        }
        WriteOut(szMsg);
        strcpy(szMsg, "\ar#\ax Dark Colors:");
        for (i = 0; i < (int)strlen(szColorChars) - 1; i++)
        {
            cCh = szColorChars[i];
            sprintf(szArg, " \a-%c[+%c+]", cCh, toupper(cCh));
            strcat(szMsg, szArg);
        }
        WriteOut(szMsg);
        WriteOut("\ar#\ax [+x+] and [+X+] set back to default color.");
    }
    else if (!strnicmp(szArg, szCmdChannels, sizeof(szCmdChannels)))
    {
        EQBC->HandleChannels(szLine);
    }
    else if (!strnicmp(szArg, szCmdNoReconnect, sizeof(szCmdNoReconnect)))
    {
        if (!EQBC->LastSecs)
        {
            WriteOut("\ar#\ax You are not trying to reconnect");
            return;
        }
        WriteOut("\ar#\ax Disabling reconnect mode for now.");
        EQBC->LastSecs = 0;
    }
    else if (!strnicmp(szArg, szCmdForceConnect, sizeof(szCmdForceConnect)))
    {
        EQBC->Connect(szLine, true);
    }
    else if (!strnicmp(szArg, szCmdIniConnect, sizeof(szCmdIniConnect)))
    {
        GetArg(szArg, szLine, 2);
        EQBC->ConnectINI(szArg);
    }
    else
    {
        sprintf(szMsg, "\ar#\ax Unsupported command, supported commands are: %s", VALID_COMMANDS);
        WriteOut(szMsg);
    }
}

// bc
void BcCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    EQBC->BC(szLine);
}

// bct
void BctCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    EQBC->BCT(szLine);
}

// bca
void BcaCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    EQBC->BCA(szLine);
}

// bcaa
void BcaaCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    EQBC->BCAA(pLPlayer, szLine);
}

// bcclear
void ClearWndCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    WINDOW->Clear();
}

// bcfont
void WndFontCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    char szArg[MAX_STRING] = {0};
    GetArg(szArg, szLine, 1);
    if (*szArg)
    {
        char* pNotNum = NULL;
        int   iValid  = strtoul(szArg, &pNotNum, 10);
        if (iValid >= 0 && iValid <= 10 && !*pNotNum)
        {
            WINDOW->NewFont(iValid);
            return;
        }
    }
    char szError[200] = {0};
    sprintf(szError, "\ar#\ax Usage: /bcfont 0-10", MODULE_NAME);
    WriteOut(szError);
}

// bcmin
void MinWndCmd(PSPAWNINFO pLPlayer, char* szLine)
{
    WINDOW->Min();
}

void KeybindEQBC(char* szKeyName, int iDown)
{
    WINDOW->Keybind(iDown);
}

// --------------------------------------
// Exported Functions

// Plugin & MQ2NetBots Support
// note: dont use netbots
PLUGIN_API void NetBotSendMsg(char* szMsg)
{
    if (szMsg && *szMsg)
    {
        EQBC->Transmit(true, "\tNBMSG");
        EQBC->Transmit(true, szMsg);
    }
}

PLUGIN_API void NetBotRequest(char* szMsg)
{
    if (szMsg && *szMsg)
    {
        if (!strncmp(szMsg, "NAMES", 5))
        {
            EQBC->Transmit(true, "\tNBNAMES");
        }
    }
}

PLUGIN_API unsigned short isConnected()
{
    return (EQBC->Connected ? 1 : 0);
}

// MQ2Main
PLUGIN_API void OnPulse()
{
    if (ValidIngame() && InHoverState()) WINDOW->Hover();
    EQBC->Pulse();
}

PLUGIN_API void SetGameState(unsigned long ulGameState)
{
    if (ulGameState == GAMESTATE_INGAME)
    {
        // setup new char name on zone (for INI writes)
        sprintf(szCharName, "%s.%s", EQADDR_SERVERNAME, ((PCHARINFO)pCharData)->Name);
        // load ini settings if entering to world
        if (!SET->Loaded)
        {
            // load ini settings
            SET->LoadINI();
            // draw window if desired
            WINDOW->Create();
            // update EQBC player name
            SetPlayer();
            // display welcome first on first entry to world
            if (!SET->FirstLoad)
            {
                char szTemp[MAX_STRING] = {0};
                sprintf(szTemp, "\ar#\ax Welcome to \ayMQ2EQBC\ax, %s: Use \ar/bccmd help\ax to see help.", szToonName);
                WriteOut(szTemp);
                SET->FirstLoad = true;
            }
        }
        // else redraw window if needed
        else
        {
            WINDOW->Create();
        }
        // handle auto connection
        EQBC->AutoConnect();
        WINDOW->ResetKeys();
    }
    else
    {
        if (ulGameState == GAMESTATE_CHARSELECT)
        {
            // kill window at char select
            WINDOW->Destroy(true);
            // set INI settings to reload on next login
            SET->Loaded = false;
            // disconnect if not logging back in
            EQBC->Logout();
            return;
        }
        // (server select) 0 && -1 normally leave connection open if '/camp server'
        if (ulGameState == -1) // wtb GAMESTATE_SERVERSELECT
        {
            WINDOW->Destroy(false);
            SET->Loaded = false;
            EQBC->Logout();
        }
    }
}

PLUGIN_API void OnCleanUI()
{
    WINDOW->Destroy(true);
}

PLUGIN_API void OnReloadUI()
{
    WINDOW->Create();
}

PLUGIN_API unsigned long OnIncomingChat(char* szLine, unsigned long ulColor)
{
    if (!ValidIngame() || !EQBC->Connected || szLine[0] != '\x12') return 0;

    PSPAWNINFO    pLPlayer               = (PSPAWNINFO)pLocalPlayer;
    char          szSender[MAX_STRING]   = {0};
    char          szTell[MAX_STRING]     = {0};
    char          szOutgoing[MAX_STRING] = {0};
    char*         pszText                = NULL;
    unsigned long n                      = strchr(&szLine[2], '\x12') - &szLine[2];

    strncpy(szSender, &szLine[2], n);

    if (SET->WatchTell && ulColor == USERCOLOR_TELL)
    {
        pszText = GetNextArg(szLine, 1, FALSE, '\'');
        strcpy(szTell, pszText);
        szTell[strlen(pszText) - 1] = '\0';
        sprintf(szOutgoing, "Tell from %s: %s", szSender, szTell);
        EQBC->BC(szOutgoing);
        return 0;
    }
    if (SET->WatchGuild && ulColor == USERCOLOR_GUILD)
    {
        pszText = GetNextArg(szLine, 1, FALSE, '\'');
        strcpy(szTell, pszText);
        szTell[strlen(pszText) - 1] = '\0';
        sprintf(szOutgoing, "GU from %s: %s", szSender, szTell);
        EQBC->BC(szOutgoing);
        return 0;
    }
    if (SET->WatchGroup && ulColor == USERCOLOR_GROUP)
    {
        pszText = GetNextArg(szLine, 1, FALSE, '\'');
        strcpy(szTell, pszText);
        szTell[strlen(pszText) - 1] = '\0';
        sprintf(szOutgoing, "GSAY from %s: %s", szSender, szTell);
        EQBC->BC(szOutgoing);
        return 0;
    }
    if (SET->WatchFsay && ulColor == USERCOLOR_FELLOWSHIP)
    {
        pszText = GetNextArg(szLine, 1, FALSE, '\'');
        strcpy(szTell, pszText);
        szTell[strlen(pszText) - 1] = '\0';
        sprintf(szOutgoing, "FSAY from %s: %s", szSender, szTell);
        EQBC->BC(szOutgoing);
        return 0;
    }
    return 0;
}

PLUGIN_API void InitializePlugin()
{
    AddCommand("/bc",      BcCmd);
    AddCommand("/bct",     BctCmd);
    AddCommand("/bca",     BcaCmd);
    AddCommand("/bcaa",    BcaaCmd);
    AddCommand("/bccmd",   BccmdCmd);
    AddCommand("/bcclear", ClearWndCmd);
    AddCommand("/bcfont",  WndFontCmd);
    AddCommand("/bcmin",   MinWndCmd);

    SET       = new CSettingsMgr();
    WINDOW    = new CEQBCWndHandler();
    EQBC      = new CConnectionMgr();
    pEQBCType = new EQBCType;

    AddMQ2Data("EQBC", dataEQBC);
    AddMQ2KeyBind("EQBC", KeybindEQBC);

    EQBC->NewThread();
}

PLUGIN_API void ShutdownPlugin()
{
    if (EQBC->Connected)
    {
        WriteOut("\ar#\ax You are still connected! Attempting to disconnect.");
        DebugSpew("MQ2Eqbc::Still Connected::Attempting disconnect.");
        EQBC->Disconnect(false);
    }

    RemoveCommand("/bc");
    RemoveCommand("/bct");
    RemoveCommand("/bca");
    RemoveCommand("/bcaa");
    RemoveCommand("/bccmd");
    RemoveCommand("/bcclear");
    RemoveCommand("/bcfont");
    RemoveCommand("/bcmin");

    RemoveMQ2Data("EQBC");
    RemoveMQ2KeyBind("EQBC"); 
    WINDOW->Destroy(ValidIngame());

    // make sure we're not trying to connect...
    EQBC->KillThread();

    delete pEQBCType;
    delete EQBC;
    delete WINDOW;
    delete SET;
}
