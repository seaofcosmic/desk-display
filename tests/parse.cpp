// Feeds bytes from a file/pty into the firmware's FrameParser and prints what it sees.
#include "../desk-display/firmware/DeskDisplay/protocol.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
int main(int argc, char** argv) {
  int fd = argc > 1 ? (strncmp(argv[1], "fd:", 3) == 0 ? atoi(argv[1] + 3) : open(argv[1], O_RDWR | O_NOCTTY)) : 0;
  bool reply = argc > 2;          // act like the device: answer hello, send a command later
  static uint8_t art[ART_BYTES];
  FrameParser p(art);
  uint8_t buf[300]; ssize_t n; int frames = 0; bool sentNext = false;
  while ((n = read(fd, buf, sizeof buf)) > 0) {
    for (ssize_t i = 0; i < n; ++i) if (p.feed(buf[i])) {
      ++frames;
      uint8_t t = p.type();
      if (t == 'M') { char tmp[600]; strcpy(tmp, (const char*)p.payload()); for (char* c = tmp; *c; ++c) if (*c == '\n') *c = '|'; printf("M ok=%d %s\n", p.ok(), tmp); }
      else if (t == 'S') { const uint8_t* q = p.payload(); printf("S ok=%d flags=%d pos=%u dur=%u\n", p.ok(), q[0], readU32(q+1), readU32(q+5)); }
      else if (t == 'A') { uint32_t s = 0; for (uint32_t k = 0; k < ART_BYTES; ++k) s += art[k]; printf("A ok=%d len=%u bytesum=%u first=%02x%02x\n", p.ok(), p.length(), s, art[0], art[1]); }
      else printf("%c ok=%d len=%u\n", t, p.ok(), p.length());
      fflush(stdout);
      if (reply && t == 'H') { if (write(fd, "junk\nDT:READY:1\n", 16) < 0) return 1; }
      if (reply && t == 'A' && !sentNext) { sentNext = true; if (write(fd, "DT:CMD:NEXT\n", 12) < 0) return 1; }
      if (reply && frames > 40) return 0;
    }
  }
  return 0;
}
