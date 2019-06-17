#ifndef __H_DISCORD__
#define __H_DISCORD__
#include "../cgame/cg_public.h"
#include "discord_rpc.h"

#define APPLICATION_ID "589822199930683413"

void CL_DiscordInit(void);
void CL_DiscordUpdatePresence(const char *state, const char *details, const char *largeImageKey, 
                            const char *largeImageText, const char *smallImageKey, const char *smallImageText, 
                            int partySize, int partyMax, int startTimestamp);
void CL_DiscordClearPresence(void);
void CL_DiscordShutdown(void);
void CL_DiscordUpdate(void);
#endif