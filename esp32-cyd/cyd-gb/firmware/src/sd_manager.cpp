#include "sd_manager.h"
#include "hw_config.h"
#include "emulator_bridge.h"
#include "i18n.h"
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

static SPIClass sdSPI(VSPI);
static bool ready = false;

bool sd_init() {
    sdSPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    if(!SD.begin(SD_PIN_CS, sdSPI, 20000000)){Serial.println("[SD] Mount fail!");return false;}
    Serial.printf("[SD] Type:%d Size:%lluMB\n",SD.cardType(),SD.cardSize()/(1024*1024));
    if(!SD.exists(ROM_PATH_GB)) SD.mkdir(ROM_PATH_GB);
    if(!SD.exists(ROM_PATH_GBC)) SD.mkdir(ROM_PATH_GBC);
    if(!SD.exists(SAVE_PATH)) SD.mkdir(SAVE_PATH);
    if(!SD.exists(CONFIG_PATH)) SD.mkdir(CONFIG_PATH);
    ready=true; return true;
}

void sd_config_defaults(CydGbConfig* cfg) {
    cfg->palette = 0;
    cfg->frame_skip = 0;
    cfg->brightness = 255;
    cfg->language = LANG_EN;
    cfg->cal_valid = false;
    cfg->cal_xmin = 3700;
    cfg->cal_xmax = 200;
    cfg->cal_ymin = 300;
    cfg->cal_ymax = 3900;
}

static int parse_key_int(const char* line, const char* key) {
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0 || line[n] != '=') return -1;
    return atoi(line + n + 1);
}

bool sd_load_config(CydGbConfig* cfg) {
    if (!ready || !cfg) return false;
    sd_config_defaults(cfg);
    if (!SD.exists(CONFIG_FILE)) {
        Serial.println("[SD] Config not found (using defaults)");
        return false;
    }
    File f = SD.open(CONFIG_FILE, FILE_READ);
    if (!f) {
        Serial.println("[SD] Config open fail");
        return false;
    }
    while (f.available()) {
        char line[48];
        int n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        char* p = line;
        while (*p == ' ' || *p == '\r') p++;

        int v;
        if ((v = parse_key_int(p, "pal")) >= 0) cfg->palette = (uint8_t)min(v, NUM_PALETTES - 1);
        else if ((v = parse_key_int(p, "fskip")) >= 0) cfg->frame_skip = (uint8_t)min(v, 4);
        else if ((v = parse_key_int(p, "bright")) >= 0) cfg->brightness = (uint8_t)v;
        else if ((v = parse_key_int(p, "lang")) >= 0) cfg->language = (uint8_t)min(v, 2);
        else if ((v = parse_key_int(p, "cal")) >= 0) cfg->cal_valid = (v != 0);
        else if ((v = parse_key_int(p, "xmin")) >= 0) cfg->cal_xmin = (int16_t)v;
        else if ((v = parse_key_int(p, "xmax")) >= 0) cfg->cal_xmax = (int16_t)v;
        else if ((v = parse_key_int(p, "ymin")) >= 0) cfg->cal_ymin = (int16_t)v;
        else if ((v = parse_key_int(p, "ymax")) >= 0) cfg->cal_ymax = (int16_t)v;
    }
    f.close();
    Serial.printf("[SD] Config loaded pal=%u fskip=%u bright=%u lang=%u cal=%d\n",
                  cfg->palette, cfg->frame_skip, cfg->brightness, cfg->language, cfg->cal_valid);
    return true;
}

bool sd_save_config(const CydGbConfig* cfg) {
    if (!ready || !cfg) return false;
    if (!SD.exists(CONFIG_PATH)) SD.mkdir(CONFIG_PATH);
    File f = SD.open(CONFIG_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[SD] Config write fail");
        return false;
    }
    f.printf("ver=1\n");
    f.printf("pal=%u\n", cfg->palette);
    f.printf("fskip=%u\n", cfg->frame_skip);
    f.printf("bright=%u\n", cfg->brightness);
    f.printf("lang=%u\n", cfg->language);
    f.printf("cal=%d\n", cfg->cal_valid ? 1 : 0);
    f.printf("xmin=%d\n", cfg->cal_xmin);
    f.printf("xmax=%d\n", cfg->cal_xmax);
    f.printf("ymin=%d\n", cfg->cal_ymin);
    f.printf("ymax=%d\n", cfg->cal_ymax);
    f.close();
    Serial.printf("[SD] Config saved -> %s\n", CONFIG_FILE);
    return true;
}

static int scan_dir(const char* dir, bool gbc, RomEntry* l, int si, int mx) {
    int c=si; File d=SD.open(dir); if(!d||!d.isDirectory()) return c;
    File e;
    while((e=d.openNextFile())&&c<mx) {
        if(e.isDirectory()){e.close();continue;}
        String n=e.name(); String lo=n; lo.toLowerCase();
        if(lo.endsWith(".gb")||lo.endsWith(".gbc")){
            strncpy(l[c].filename,n.c_str(),MAX_FILENAME-1);
            snprintf(l[c].full_path,80,"%s/%s",dir,n.c_str());
            l[c].size=e.size(); l[c].is_gbc=gbc; c++;
        }
        e.close();
    }
    d.close(); return c;
}

int sd_scan_roms(RomEntry* l, int mx) {
    if(!ready) return 0;
    int c=scan_dir(ROM_PATH_GB,false,l,0,mx);
    c=scan_dir(ROM_PATH_GBC,true,l,c,mx);

    for(int i=0;i<c-1;i++) for(int j=i+1;j<c;j++)
        if(strcasecmp(l[i].filename,l[j].filename)>0){RomEntry t=l[i];l[i]=l[j];l[j]=t;}
    Serial.printf("[SD] Found %d ROMs\n",c);
    return c;
}

bool sd_load_rom(const char* p, uint8_t** buf, uint32_t* sz) { return false;  }
void sd_free_rom(uint8_t* b) { if(b) free(b); }

void sd_get_save_path(const char* rp, char* sp, int mx) {
    const char* fn=strrchr(rp,'/'); if(!fn)fn=rp; else fn++;
    char base[MAX_FILENAME]; strncpy(base,fn,MAX_FILENAME-1); base[MAX_FILENAME-1]=0;
    char* dot=strrchr(base,'.'); if(dot)*dot=0;
    snprintf(sp,mx,"%s/%s.sav",SAVE_PATH,base);
}

bool sd_save_state(const char* rp, const uint8_t* data, uint32_t sz) {
    if(!ready||!data||!sz) return false;
    char sp[96]; sd_get_save_path(rp,sp,96);
    File f=SD.open(sp,FILE_WRITE); if(!f) return false;
    size_t w=f.write(data,sz); f.close();
    Serial.printf("[SD] Save: %s (%u)\n",sp,w);
    return w==sz;
}

bool sd_load_state(const char* rp, uint8_t* data, uint32_t sz) {
    if(!ready||!data||!sz) return false;
    char sp[96]; sd_get_save_path(rp,sp,96);
    if(!SD.exists(sp)) return false;
    File f=SD.open(sp,FILE_READ); if(!f) return false;
    size_t r=f.read(data,sz); f.close();
    Serial.printf("[SD] Load: %s (%u)\n",sp,r);
    return r==sz;
}
