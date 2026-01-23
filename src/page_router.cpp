#include <string.h>
#include "page_router.h"
#include "web_pages.h"

static const Page pages[] = {
  { "/board-edit.html", BOARD_EDIT_HTML_GZ, BOARD_EDIT_HTML_GZ_LEN, "text/html", true },
  { "/board-view.html", BOARD_VIEW_HTML_GZ, BOARD_VIEW_HTML_GZ_LEN, "text/html", true },
  { "/game.html", GAME_HTML_GZ, GAME_HTML_GZ_LEN, "text/html", true },
  { "/", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN, "text/html", true },
  { "/pieces/bB.svg", BB_SVG_GZ, BB_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/bK.svg", BK_SVG_GZ, BK_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/bN.svg", BN_SVG_GZ, BN_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/bP.svg", BP_SVG_GZ, BP_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/bQ.svg", BQ_SVG_GZ, BQ_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/bR.svg", BR_SVG_GZ, BR_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wB.svg", WB_SVG_GZ, WB_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wK.svg", WK_SVG_GZ, WK_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wN.svg", WN_SVG_GZ, WN_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wP.svg", WP_SVG_GZ, WP_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wQ.svg", WQ_SVG_GZ, WQ_SVG_GZ_LEN, "image/svg+xml", true },
  { "/pieces/wR.svg", WR_SVG_GZ, WR_SVG_GZ_LEN, "image/svg+xml", true },
  { "/styles.css", STYLES_CSS_GZ, STYLES_CSS_GZ_LEN, "text/css", true },
};

const Page *findPage(const char *path)
{
  size_t plen = strlen(path);

  for (auto &p : pages)
  {
    const char *ppath = p.path;
    size_t flen = strlen(ppath);

    // 1) Exact match: "/foo.js" == "/foo.js"
    if (plen == flen && memcmp(path, ppath, plen) == 0)
      return &p;

    // 2) Extensionless match: "/foo" == "/foo.<ext>"
    // Find '.' in stored path
    const char *dot = strchr(ppath, '.');
    if (!dot)
      continue;

    size_t baseLen = dot - ppath;

    if (plen == baseLen && memcmp(path, ppath, baseLen) == 0)
      return &p;
  }

  return nullptr;
}
