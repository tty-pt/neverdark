#ifndef _MCPPKG_H
#define _MCPPKG_H

void show_mcp_error(McpFrame * mfr, char *topic, char *text);
void mcppkg_simpleedit(McpFrame * mfr, McpMesg * msg, McpVer ver, void *context);
void mcppkg_help_request(McpFrame * mfr, McpMesg * msg, McpVer ver, void *context);

#endif /* _MCPPKG_H */

#ifdef DEFINE_HEADER_VERSIONS


const char *mcppkg_h_version = "$RCSfile$ $Revision: 1.6 $";

#else
extern const char *mcppkg_h_version;
#endif

