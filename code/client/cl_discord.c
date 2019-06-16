#define DISCORD_DYNAMIC_LIB

#include "discord_rpc.h"
#include "client.h"

static void handleDiscordReady(const DiscordUser *connectedUser)
{
    Com_Printf("Discord: connected to user %s#%s - %s\n",
           connectedUser->username,
           connectedUser->discriminator,
           connectedUser->userId);
}

static void handleDiscordDisconnected(int errcode, const char* message)
{
    Com_Printf("Discord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message)
{
    Com_Printf("Discord: error (%d: %s)\n", errcode, message);
}

static void handleDiscordJoin(const char* secret)
{
    Com_Printf("Discord: join (%s)\n", secret);
    Cbuf_ExecuteTextSafe(EXEC_APPEND, secret);
}

static void handleDiscordSpectate(const char* secret)
{
    Com_Printf("Discord: spectate (%s)\n", secret);
}

static void handleDiscordJoinRequest(const DiscordUser* request)
{    
    Com_Printf("Discord: join request from %s#%s - %s\n",
           request->username,
           request->discriminator,
           request->userId);

    Discord_Respond(request->userId, DISCORD_REPLY_YES);
}

int running = 0;

void CL_DiscordInit(void)
{
    Com_Printf("Discord: Initializing\n");
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
}

static const char *gameStateStrings[] = {
    "Main Menu",
	"Loading",
    "Singleplayer",
    "Multiplayer"
};

void CL_DiscordUpdatePresence(gamestateDiscord_t gameState, const char *mapname, const char *gametype, int numPlayers, int maxPlayers, int startTime)
{ 
    if (running == 0)
        return;

    Com_Printf("Discord: Updating %d %s %s %d %d %d\n", gameState, mapname, gametype, numPlayers, maxPlayers, startTime);

    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));

    discordPresence.state = gameStateStrings[gameState];
    discordPresence.details = va("%s on %s", gametype, mapname);
    discordPresence.startTimestamp = startTime;
    discordPresence.largeImageKey = mapname;
    discordPresence.largeImageText = mapname;
    discordPresence.smallImageKey = "q3";
    discordPresence.partyId = va("server_%s", NET_AdrToStringwPort(clc.serverAddress));
    discordPresence.partySize = 1;
    discordPresence.partyMax = maxPlayers;
    discordPresence.joinSecret = NET_AdrToStringwPort(clc.serverAddress);
    Discord_UpdatePresence(&discordPresence);
}

void CL_DiscordClearPresence(void)
{
    if (running == 0)
        return;

    Com_Printf("Discord: Clearing\n");

    Discord_ClearPresence();
}

void CL_DiscordShutdown(void)
{
    if (running == 0)
        return;

    Com_Printf("Discord: Shuting down\n");

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