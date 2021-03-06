#include <sys/types.h>

#include "CmdlineScreen.h"

/*{
#include "ProcessList.h"
#include "Panel.h"
#include "FunctionBar.h"

typedef struct CmdlineScreen_ {
   Process* process;
   Panel* display;
   FunctionBar* bar;
} CmdlineScreen;
}*/

static char* ofsFunctions[] = {"Refresh", "Done   ", NULL};

static char* ofsKeys[] = {"F5", "Esc"};

static int ofsEvents[] = {KEY_F(5), 27};

CmdlineScreen* CmdlineScreen_new(Process* process) {
   CmdlineScreen* this = (CmdlineScreen*) malloc(sizeof(CmdlineScreen));
   this->process = process;
   this->display = Panel_new(0, 1, COLS, LINES-3, LISTITEM_CLASS, true, ListItem_compare);
   this->bar = FunctionBar_new(ofsFunctions, ofsKeys, ofsEvents);
   return this;
}

void CmdlineScreen_delete(CmdlineScreen* this) {
   Panel_delete((Object*)this->display);
   FunctionBar_delete((Object*)this->bar);
   free(this);
}

static void CmdlineScreen_draw(CmdlineScreen* this) {
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "command line of process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true);
   FunctionBar_draw(this->bar, NULL);
}

static void CmdlineScreen_scan(CmdlineScreen* this) {
   Panel* panel = this->display;
   int idx = MAX(Panel_getSelectedIndex(panel), 0);
   int mib[3];
   int argmax;
   size_t bufsz = sizeof(argmax);

   Panel_prune(panel);
   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;
   if (sysctl(mib, 2, &argmax, &bufsz, 0, 0) == 0)
   {
      char* buf = malloc(argmax);
      if (buf)
      {
         mib[1] = KERN_PROCARGS2;
         mib[2] = this->process->pid;
         bufsz = argmax;
         if (sysctl(mib, 3, buf, &bufsz, 0, 0) == 0)
         {
            if (bufsz > sizeof(int))
            {
               char *p = buf, *endp = buf + bufsz;
               int argc = *(int*)p;
               p += sizeof(int);

               // skip exe
               p = strchr(p, 0)+1;

               // skip padding
               while(!*p && p < endp)
                  ++p;

               for (; argc-- && p < endp; p = strrchr(p, 0)+1)
                  Panel_add(panel, (Object*) ListItem_new(p, 0));
            }
            else
            {
               Panel_add(panel, (Object*) ListItem_new("Could not allocate memory.", 0));
            }
         }
         else
         {
            Panel_add(panel, (Object*) ListItem_new("sysctl(KERN_PROCARGS2) failed.", 0));
         }
         free(buf);
      }
      else
      {
         Panel_add(panel, (Object*) ListItem_new("sysctl(KERN_ARGMAX) failed.", 0));
      }
   }
   Panel_setSelected(panel, idx);
}

void CmdlineScreen_run(CmdlineScreen* this) {
   Panel* panel = this->display;
   Panel_setHeader(panel, " ");
   CmdlineScreen_scan(this);
   CmdlineScreen_draw(this);
   //CRT_disableDelay();

   bool looping = true;
   while (looping) {
      Panel_draw(panel, true);
      int ch = getch();
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = FunctionBar_synthesizeEvent(this->bar, mevent.x);
      }
      switch(ch) {
      case ERR:
         continue;
      case KEY_F(5):
         clear();
         CmdlineScreen_scan(this);
         CmdlineScreen_draw(this);
         break;
      case '\014': // Ctrl+L
         clear();
         CmdlineScreen_draw(this);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         CmdlineScreen_draw(this);
         break;
      default:
         Panel_onKey(panel, ch);
      }
   }
   //CRT_enableDelay();
}
