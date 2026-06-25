#include "emulator_bridge.h"
#include "display.h"
#include "hw_config.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <string.h>
#include <SD.h>
#include <SPIFFS.h>

#define ENABLE_LCD 1
#define ENABLE_SOUND 1

#include "minigb_apu.h"

static struct minigb_apu_ctx apu;
static audio_sample_t audio_buf[AUDIO_SAMPLES_TOTAL];

static uint8_t audio_read(const uint16_t addr) {
    return minigb_apu_audio_read(&apu, addr);
}

static void audio_write(const uint16_t addr, const uint8_t val) {
    minigb_apu_audio_write(&apu, addr, val);
}

#define PEANUT_GB_HIGH_LCD_ACCURACY 0
#include "peanut_gb.h"
#include "audio_output.h"
#include "touch_input.h"


#define PG_SZ 4096
#define PG_N  16
#define PG_MASK (PG_SZ-1)
#define HASH_SZ 32
#define HASH_M (HASH_SZ-1)

struct Pg { uint32_t addr, acc; uint8_t* d; bool v; };
static Pg pg[PG_N];
static int8_t ht[HASH_SZ];
static uint32_t acc = 0;
static int npg = 0;
static File romf;
static uint32_t romlen = 0;

#define B0SZ (32*1024)
static uint8_t* b0 = nullptr;

static inline uint8_t* IRAM_ATTR cget(uint32_t a) {
    uint32_t pb = a & ~PG_MASK;
    int8_t i = ht[(pb>>12)&HASH_M];
    if (i >= 0 && pg[i].v && pg[i].addr == pb) { pg[i].acc = ++acc; return &pg[i].d[a&PG_MASK]; }
    for (int j=0;j<npg;j++) if (pg[j].v && pg[j].addr==pb) {
        pg[j].acc=++acc; ht[(pb>>12)&HASH_M]=j; return &pg[j].d[a&PG_MASK];
    }
    int lru=0; uint32_t old=UINT32_MAX;
    for (int j=0;j<npg;j++) { if (!pg[j].v){lru=j;break;} if(pg[j].acc<old){old=pg[j].acc;lru=j;} }
    if (pg[lru].v) { int8_t oh=(pg[lru].addr>>12)&HASH_M; if(ht[oh]==lru) ht[oh]=-1; }
    romf.seek(pb); size_t r=romf.read(pg[lru].d, min((uint32_t)PG_SZ,romlen-pb));
    if (r<PG_SZ) memset(pg[lru].d+r,0xFF,PG_SZ-r);
    pg[lru].addr=pb; pg[lru].acc=++acc; pg[lru].v=true; ht[(pb>>12)&HASH_M]=lru;
    return &pg[lru].d[a&PG_MASK];
}


static struct gb_s* gb = nullptr;
#define MAXRAM (32*1024)
static uint8_t* cram = nullptr;
static uint16_t lbuf[GB_SCREEN_W];
static uint8_t fskip = 0, fcnt = 0;
static uint32_t fpsc = 0, fpst = 0, cfps = 0;
static volatile uint8_t jpad = 0;

struct PalRGB { uint8_t r, g, b; };

static const PalRGB pal_src[NUM_PALETTES][4] = {
    {{155,188, 15},{139,172, 15},{ 48, 98, 48},{ 15, 56, 15}},
    {{224,224,216},{160,160,152},{ 96, 96, 88},{ 32, 32, 24}},
    {{255,240,200},{220,180,120},{160,100, 60},{ 80, 48, 24}},
    {{180,220,255},{120,170,230},{ 60,110,180},{ 20, 50,100}},
    {{255,200,120},{220,140, 60},{160, 80, 30},{ 80, 40, 10}},
    {{255,120, 80},{220, 60, 40},{160, 20, 10},{ 80,  0,  0}},
    {{ 80,180,255},{ 40,120,200},{ 20, 70,140},{  0, 30, 80}},
    {{140,220,100},{ 90,170, 60},{ 50,110, 30},{ 20, 60, 10}},
    {{255,180,100},{230,120, 60},{180, 70, 30},{120, 30,  0}},
    {{255,140,160},{220, 80,100},{160, 40, 60},{100,  0, 20}},
    {{200,240,255},{140,200,230},{ 80,150,190},{ 30, 80,120}},
    {{180,120, 80},{140, 80, 50},{100, 50, 25},{ 60, 25, 10}},
    {{180,255,200},{120,220,150},{ 70,170,100},{ 30,100, 50}},
    {{220,180,255},{170,130,220},{120, 80,170},{ 70, 40,110}},
    {{255,220, 80},{220,170, 40},{170,120, 10},{120, 80,  0}},
    {{200,210,255},{140,155,220},{ 80, 95,160},{ 30, 40, 90}},
    {{200,160,255},{150,110,220},{100, 60,160},{ 55, 25,100}},
    {{220,160,120},{170,110, 70},{120, 65, 35},{ 70, 35, 15}},
    {{255,200,160},{220,150,100},{170, 90, 50},{100, 45, 15}},
    {{160,180,220},{110,130,190},{ 65, 85,140},{ 30, 45, 90}},
    {{255,240,100},{240,200, 40},{190,150, 15},{110, 85,  0}},
    {{ 80,255,255},{ 40,210,240},{ 20,140,200},{ 10, 70,120}},
    {{255,100,120},{190, 55, 75},{110, 25, 45},{ 55, 10, 20}},
    {{255,240,120},{120,255,180},{255,150, 90},{ 90, 80,200}},
    {{255,220,240},{200,255,220},{200,210,255},{130, 90,170}},
    {{255,255, 80},{  0,255,240},{255,  0,200},{ 80,  0,160}},
    {{255,200,255},{140,180,255},{ 80,120,220},{ 40, 30,100}},
    {{255,220,240},{255,140,180},{200, 60,100},{ 80, 20, 80}},
    {{180,220,255},{100,210,100},{220,140, 80},{ 60, 40, 30}},
    {{255,240,160},{120,200, 80},{200,160, 60},{ 50, 80, 50}},
    {{255,240,220},{255,120,120},{120,160,255},{ 60, 30, 90}},
    {{255,240,100},{255,100,100},{100,220,100},{120, 60,200}},
    {{255,240,200},{255,120, 60},{ 80,160,255},{ 20, 40,120}},
    {{255,255,160},{ 80,255,220},{255,120,200},{ 40,140, 70}},
    {{255,200,240},{180,140,255},{100,180,255},{ 50, 50,140}},
    {{255,220,180},{140,220,160},{100,180,220},{ 40, 60,100}},
    {{255,180,255},{200,120,255},{140, 80,210},{ 70, 40,120}},
    {{255, 60,180},{180, 40,255},{ 60,200,255},{ 30, 20, 80}},
    {{255,  0,255},{  0,255,255},{255,255,  0},{255,  0,  0}},
    {{120,200,255},{ 80,200,120},{200,120, 80},{ 60, 40,120}},
    {{180,255,220},{140,200,255},{200,140,255},{ 50, 60,100}},
    {{180,220,200},{120,180,150},{ 70,120, 90},{ 35, 70, 45}},
    {{210,255,  0},{100,220,180},{255,140,  0},{ 80, 60,180}},
    {{255,140,255},{210, 90,210},{150, 50,150},{ 90, 15, 90}},
    {{255,200,120},{120,220,255},{220,100,180},{ 60, 40, 90}},
    {{255,180,100},{100,255,200},{200,100,255},{ 80, 40,120}},
    {{200,255,255},{255,200,120},{255,120,180},{ 60, 80,160}},
    {{255,240,200},{180,255,140},{140,180,255},{ 60, 50,120}},
    {{255,220,160},{255,160,200},{160,200,255},{ 70, 50,130}},
    {{240,255,180},{180,255,220},{220,180,255},{ 80,100,160}},
    {{255,255,200},{255,180,120},{120,200,255},{ 50, 50,130}},
    {{255,200,180},{180,255,200},{180,180,255},{100, 80,160}},
};

static const char* palnames[NUM_PALETTES] = {
    "Classic Green","Original DMG","Warm Sepia","Cool Blue",
    "Autumn","Lava","Ocean","Forest",
    "Sunset","Cherry","Ice","Chocolate",
    "Mint","Lavender","Gold","Midnight",
    "Grape","Rust","Copper","Denim",
    "Sunflower","Electric","Blood Moon",
    "Rainbow","Pastel Quad","Arcade","Cosmos",
    "Kirby","Mario","Zelda","Poke",
    "Fruit","Fire Ice","Tropical","Berry GBC",
    "Links","Vaporwave","Synthwave","Neon",
    "Duck Hunt","Aurora","Patina","Toxic Pop",
    "Magenta","Candy","Sunset Wave","Parrot",
    "Spring","Cotton Candy","Opal","Beach",
    "Watercolor"
};

static uint16_t pals[NUM_PALETTES][3][4];
static uint8_t curpal = 0;
static bool pals_ready = false;

static uint16_t make565(uint8_t r, uint8_t g, uint8_t b) {
    return tft.color565(r, g, b);
}

static uint16_t dim565(uint16_t c, uint8_t num, uint8_t den) {
    if (den == 0) return c;
    uint8_t r = ((c >> 11) & 0x1F) * 255 / 31 * num / den;
    uint8_t g = ((c >> 5) & 0x3F) * 255 / 63 * num / den;
    uint8_t b = (c & 0x1F) * 255 / 31 * num / den;
    return make565(r, g, b);
}

void emu_build_palettes() {
    if (pals_ready) return;
    for (int p = 0; p < NUM_PALETTES; p++) {
        for (int s = 0; s < 4; s++) {
            uint16_t base = make565(pal_src[p][s].r, pal_src[p][s].g, pal_src[p][s].b);
            pals[p][0][s] = base;
            pals[p][1][s] = dim565(base, 82, 100);
            pals[p][2][s] = base;
        }
    }
    pals_ready = true;
    Serial.println("[EMU] Palettes built (RGB565 + layers)");
}

void emu_set_palette(uint8_t i) {
    if (!pals_ready) emu_build_palettes();
    if (i >= NUM_PALETTES) i = NUM_PALETTES - 1;
    curpal = i;
    ui_theme_apply(i);
}

uint8_t emu_get_palette() { return curpal; }
const char* emu_get_palette_name(uint8_t i) { return (i < NUM_PALETTES) ? palnames[i] : "?"; }

uint16_t emu_palette_color(uint8_t pal_idx, uint8_t shade) {
    if (!pals_ready) emu_build_palettes();
    if (pal_idx >= NUM_PALETTES) pal_idx = 0;
    if (shade > 3) shade = 3;
    return pals[pal_idx][2][shade];
}

uint16_t emu_palette_color_dim(uint8_t pal_idx, uint8_t shade, uint8_t num, uint8_t den) {
    return dim565(emu_palette_color(pal_idx, shade), num, den);
}


static uint8_t IRAM_ATTR gb_rom_read(struct gb_s* g, const uint_fast32_t a) {
    (void)g; if(a>=romlen) return 0xFF; if(a<B0SZ) return b0[a]; return *cget(a);
}
static uint8_t IRAM_ATTR gb_cram_r(struct gb_s* g, const uint_fast32_t a) {
    (void)g; return (a<MAXRAM)?cram[a]:0xFF;
}
static void IRAM_ATTR gb_cram_w(struct gb_s* g, const uint_fast32_t a, const uint8_t v) {
    (void)g; if(a<MAXRAM) cram[a]=v;
}
static void gb_err(struct gb_s* g, const enum gb_error_e e, const uint16_t a) {
    (void)g; Serial.printf("[EMU] Err %d @0x%04X\n",(int)e,a);
}
static void IRAM_ATTR lcd_line(struct gb_s* g, const uint8_t px[160], const uint_fast8_t ln) {
    (void)g;
    if (fskip>0 && (fcnt%(fskip+1))!=0) return;
    const uint16_t (*layer)[4] = pals[curpal];
    for (int x=0;x<GB_SCREEN_W;x++) {
        uint8_t v = px[x];
        uint8_t shade = v & 3;
        int li = 2;
        switch (v & 0x30) {
            case 0x00: li = 0; break;
            case 0x10: li = 1; break;
            case 0x20: li = 2; break;
        }
        lbuf[x] = layer[li][shade];
    }
    display_push_gb_line(ln, lbuf);
}


static bool cp2spiffs(const char* sp, const char* dp) {
    File s=SD.open(sp,FILE_READ); if(!s) return false;
    File d=SPIFFS.open(dp,FILE_WRITE); if(!d){s.close();return false;}
    uint8_t buf[512]; uint32_t tot=0;
    while(s.available()){size_t r=s.read(buf,512);d.write(buf,r);tot+=r;
        if(tot%65536==0) Serial.printf("[SPIFFS] %uKB\n",tot/1024);}
    d.close();s.close();
    Serial.printf("[SPIFFS] Done %u bytes\n",tot); return true;
}


bool emu_open_rom(const char* path) {
    if(!SPIFFS.begin(true)) Serial.println("[SPIFFS] format");
    String sn="/rom.gb";
    if(SPIFFS.exists(sn)){
        File sc=SD.open(path,FILE_READ); uint32_t ssz=sc?sc.size():0; if(sc)sc.close();
        if (ssz == 0) {
            SPIFFS.remove(sn);
            Serial.println("[EMU] Removed empty SPIFFS cache");
        } else {
            romf=SPIFFS.open(sn,FILE_READ);
            if(romf && romf.size()==ssz && ssz>0){
                romlen=romf.size();
                Serial.printf("[EMU] SPIFFS %uKB\n",romlen/1024);
                return true;
            }
            if(romf) romf.close();
        }
    }
    File sf=SD.open(path,FILE_READ); if(!sf) return false;
    uint32_t sz=sf.size(); sf.close();
    if (sz == 0) { Serial.println("[EMU] ROM size 0 — arquivo invalido no SD"); return false; }
    if(sz<=SPIFFS.totalBytes()-SPIFFS.usedBytes()){
        Serial.println("[EMU] Copying to SPIFFS...");
        if(SPIFFS.exists(sn)) SPIFFS.remove(sn);
        if(cp2spiffs(path,sn.c_str())){
            romf=SPIFFS.open(sn,FILE_READ);
            if(romf){romlen=romf.size();return true;}
        }
    }
    romf=SD.open(path,FILE_READ); if(!romf) return false;
    romlen=romf.size(); return true;
}
void emu_close_rom(){
    if(romf)romf.close();
    romlen=0;
#if ENABLE_SOUND
    audio_output_shutdown();
    touch_reinit();
#endif
}

bool emu_init(uint8_t*,uint32_t) {
    if(!romf||!romlen) return false;
    memset(ht,-1,sizeof(ht));
    npg=0;
    for(int i=0;i<PG_N;i++){pg[i].v=false;if(!pg[i].d)pg[i].d=(uint8_t*)malloc(PG_SZ);if(!pg[i].d)break;npg++;}
    Serial.printf("[EMU] %d pages\n",npg);
    if(!b0) b0=(uint8_t*)malloc(B0SZ); if(!b0) return false;
    romf.seek(0); romf.read(b0,min(romlen,(uint32_t)B0SZ));
    if(!cram) cram=(uint8_t*)malloc(MAXRAM); if(!cram) return false;
    memset(cram,0xFF,MAXRAM);
    if(!gb) gb=(struct gb_s*)malloc(sizeof(struct gb_s)); if(!gb) return false;
    memset(gb,0,sizeof(struct gb_s));
    enum gb_init_error_e r=gb_init(gb,gb_rom_read,gb_cram_r,gb_cram_w,gb_err,nullptr);
    if(r!=GB_INIT_NO_ERROR){Serial.printf("[EMU] init fail %d\n",(int)r);return false;}
    gb_init_lcd(gb,lcd_line);
#if ENABLE_SOUND
    minigb_apu_audio_init(&apu);
    audio_output_init();
    touch_reinit();
#endif
    fcnt=fpsc=cfps=0; fpst=millis(); acc=0;
    char t[17]={0}; for(int i=0;i<16;i++){char c=(char)b0[0x134+i];t[i]=(c>=32&&c<127)?c:0;}
    Serial.printf("[EMU] '%s' %uKB heap:%u\n",t,romlen/1024,ESP.getFreeHeap());
    return true;
}

void emu_run_frame() {
    gb->direct.joypad_bits.a=!(jpad&0x10); gb->direct.joypad_bits.b=!(jpad&0x20);
    gb->direct.joypad_bits.select=!(jpad&0x40); gb->direct.joypad_bits.start=!(jpad&0x80);
    gb->direct.joypad_bits.right=!(jpad&0x01); gb->direct.joypad_bits.left=!(jpad&0x02);
    gb->direct.joypad_bits.up=!(jpad&0x04); gb->direct.joypad_bits.down=!(jpad&0x08);
    gb_run_frame(gb);
#if ENABLE_SOUND
    minigb_apu_audio_callback(&apu, audio_buf);
    audio_output_submit(audio_buf, AUDIO_SAMPLES);
#endif
    fcnt++; fpsc++;
    uint32_t n=millis(); if(n-fpst>=1000){cfps=fpsc;fpsc=0;fpst=n;}
}

void emu_set_joypad(uint8_t b){jpad=b;}
uint8_t* emu_get_cart_ram(uint32_t* s){uint_fast32_t r=0;gb_get_save_size_s(gb,&r);if(s)*s=(uint32_t)r;return cram;}
void emu_set_cart_ram(const uint8_t* d,uint32_t s){if(s>MAXRAM)s=MAXRAM;memcpy(cram,d,s);}
void emu_set_frame_skip(uint8_t s){fskip=s;}
uint8_t emu_get_frame_skip(){return fskip;}
uint32_t emu_get_fps(){return cfps;}
uint16_t* emu_get_line_buffer(){return lbuf;}
void emu_reset(){gb_reset(gb);fcnt=0;}
