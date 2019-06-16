#ifndef __H_DISCORD__
#define __H_DISCORD__
#include "../cgame/cg_public.h"

#define APPLICATION_ID "589822199930683413"

void CL_DiscordInit(void);
void CL_DiscordUpdatePresence(gamestateDiscord_t gameState, const char *mapname, const char *gametype, int numPlayers, int maxPlayers, int startTime);
void CL_DiscordClearPresence(void);
void CL_DiscordShutdown(void);
void CL_DiscordUpdate(void);
#endif