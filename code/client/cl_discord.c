#define DISCORD_DYNAMIC_LIB

#include "client.h"

static void handleDiscordReady(const DiscordUser *connectedUser)
{
    Com_DPrintf("Discord: connected to user %s#%s - %s\n",
           connectedUser->username,
           connectedUser->discriminator,
           connectedUser->userId);
}

static void handleDiscordDisconnected(int errcode, const char* message)
{
    Com_DPrintf("Discord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message)
{
    Com_Printf("Discord: error (%d: %s)\n", errcode, message);
}

static void handleDiscordJoin(const char* secret)
{
    Com_DPrintf("Discord: join (%s)\n", secret);
    Cbuf_ExecuteTextSafe(EXEC_APPEND, va("connect %s", secret));
}

static void handleDiscordSpectate(const char* secret)
{
    Com_DPrintf("Discord: spectate (%s)\n", secret);
}

static void handleDiscordJoinRequest(const DiscordUser* request)
{    
    Com_DPrintf("Discord: join request from %s#%s - %s\n",
           request->username,
           request->discriminator,
           request->userId);

    Discord_Respond(request->userId, DISCORD_REPLY_YES);
}

int running = 0;

int64_t startTime = 0;

void CL_DiscordInit(void)
{
    Com_DPrintf("Discord: Initializing\n");
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = handleDiscordReady;
    handlers.disconnected = handleDiscordDisconnected;
    handlers.errored = handleDiscordError;
    handlers.joinGame = handleDiscordJoin;
    handlers.spectateGame = handleDiscordSpectate;
    handlers.joinRequest = handleDiscordJoinRequest;
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
    running = 1;
    startTime = Sys_Epoch();
}

void CL_DiscordUpdatePresence(const char *state, const char *details, const char *largeImageKey, const char *largeImageText,
                                const char *smallImageKey, const char *smallImageText, int partySize, int partyMax, int startTimestamp)
{    
    if (running == 0)
        return;

    Com_DPrintf("Discord: Updating %s %s %s %s %s %s %d %d %d\n", state, details, 
                largeImageKey, largeImageText, smallImageKey, 
                smallImageText, partySize, partyMax, startTime);

    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));

    discordPresence.state = state;
    discordPresence.details = details;
    discordPresence.startTimestamp = startTime;
    discordPresence.largeImageKey = largeImageKey;
    discordPresence.largeImageText = largeImageText;
    discordPresence.smallImageKey = smallImageKey;
    discordPresence.smallImageText = smallImageText;
    discordPresence.partySize = partySize;
    discordPresence.partyMax = partyMax;

    if (partySize > 0)
    {
        discordPresence.partyId = va("server_%s", NET_AdrToStringwPort(clc.serverAddress));
        discordPresence.joinSecret = NET_AdrToStringwPort(clc.serverAddress);
    }

    Discord_UpdatePresence(&discordPresence);
}

void CL_DiscordClearPresence(void)
{
    if (running == 0)
        return;

    Com_DPrintf("Discord: Clearing\n");

    Discord_ClearPresence();
}

void CL_DiscordShutdown(void)
{
    if (running == 0)
        return;

    Com_DPrintf("Discord: Shuting down\n");

    running = 0;
    Discord_Shutdown();
}

void CL_DiscordUpdate(void)
{
#ifdef DISCORD_DISABLE_IO_THREAD
        Discord_UpdateConnection();
#endif
        Discord_RunCallbacks();
}