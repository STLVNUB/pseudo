#import "Global.h"


#define SAMPLE_RATE\
    44100

#define MAX_CHANNELS\
    24

#define MAX_VOLUME\
    0x3fff

#define spuAcc(addr)\
    *(uh *)&mem.hwr.ptr[addr]

#define spuChannel(addr)\
    (addr >> 4) & 0x1f

#define MAX(a, b) ((a) > (b) ? a : b)
#define MIN(a, b) ((a) < (b) ? a : b)


CstrAudio audio;

void CstrAudio::reset() {
    // Variables
    spuAddr = ~0;
    spuVolumeL = MAX_VOLUME;
    spuVolumeR = MAX_VOLUME;
    
    memset(&spuMem, 0, sizeof(hi));
    
    // Channels
    for (int n = 0; n < MAX_CHANNELS; n++) {
        memset(&spuVoices[n].buffer, 0, USHRT_MAX * 2);
        spuVoices[n].count    = 0;
        spuVoices[n].freq     = 0;
        spuVoices[n].on       = false;
        spuVoices[n].pos      = 0;
        spuVoices[n].raddr    = 0;
        spuVoices[n].saddr    = 0;
        spuVoices[n].size     = 0;
        spuVoices[n].volume.l = 0;
        spuVoices[n].volume.r = 0;
    }
}

void CstrAudio::voiceOn(uh data) {
    for (int n = 0; n < MAX_CHANNELS; n++) {
        if (data & (1 << n)) {
            spuVoices[n].on    = true;
            spuVoices[n].count = 0;
            spuVoices[n].pos   = 0;
            spuVoices[n].raddr = 0;
            spuVoices[n].size  = 0;
            
            //depackVAG(spuVoices[n]);
        }
    }
}

void CstrAudio::voiceOff(uh data) {
    for (int n = 0; n < MAX_CHANNELS; n++) {
        if (data & (1 << n)) {
            spuVoices[n].on = false;
        }
    }
}

void CstrAudio::write(uw addr, uh data) {
    // Switch to low order bits
    addr = lob(addr);
    
    spuAcc(addr) = data;
    
    // Channels
    if (addr >= 0x1c00 && addr <= 0x1d7e) {
        int n = spuChannel(addr);
        
        switch(addr & 0xf) {
            case 0x0: // Volume L
                spuVoices[n].volume.l = data & MAX_VOLUME;
                return;
                
            case 0x2: // Volume R
                spuVoices[n].volume.r = data & MAX_VOLUME;
                return;
                
            case 0x4: // Pitch
                spuVoices[n].freq = MAX((data * SAMPLE_RATE) / 4096, 1);
                return;
                
            case 0x6: // Sound Address
                spuVoices[n].saddr = data << 3;
                return;
                
            case 0xe: // Return Address
                spuVoices[n].raddr = data << 3;
                return;
                
            /* unused */
            case 0x8:
            case 0xa:
            case 0xc:
                return;
        }
        
        printx("PSeudo /// SPU write < 0x1d80 $%x <- $%x", addr, data);
    }
    
    // Reverb
    if (addr >= 0x1dc0 && addr <= 0x1dfe) {
        return;
    }
    
    // HW
    switch(addr) {
        case 0x1d80: // Volume L
            spuVolumeL = data & MAX_VOLUME;
            return;
        
        case 0x1d82: // Volume R
            spuVolumeR = data & MAX_VOLUME;
            return;
        
        case 0x1d88: // Sound On 1
            voiceOn(data);
            return;
        
        case 0x1d8a: // Sound On 2
            voiceOn(data << 16);
            return;
        
        case 0x1d8c: // Sound Off 1
            voiceOff(data);
            return;
        
        case 0x1d8e: // Sound Off 2
            voiceOff(data << 16);
            return;
        
        case 0x1da6: // Transfer Address
            spuAddr = data << 3;
            return;
            
        case 0x1da8: // Data
            spuMem.u16[spuAddr >> 1] = data;
            spuAddr += 2;
            spuAddr &= 0x3ffff;
            return;
        
        case 0x1daa: // Control
            return;
        
        case 0x1d84: // Reverb Volume L
        case 0x1d86: // Reverb Volume R
        case 0x1d90: // FM Mode On 1
        case 0x1d92: // FM Mode On 2
        case 0x1d94: // Noise Mode On 1
        case 0x1d96: // Noise Mode On 2
        case 0x1d98: // Reverb Mode On 1
        case 0x1d9a: // Reverb Mode On 2
        case 0x1da2: // Reverb Address
        case 0x1dac:
        case 0x1db0: // CD Volume L
        case 0x1db2: // CD Volume R
        case 0x1db4:
        case 0x1db6:
            return;
    }
    
    printx("PSeudo /// SPU write: $%x <- $%x", addr, data);
}

uh CstrAudio::read(uw addr) {
    // Switch to low order bits
    addr = lob(addr);
    
    // Channels
    if (addr >= 0x1c00 && addr <= 0x1d7e) {
        switch(addr&0xf) {
            case 0x8:
            case 0xa:
            case 0xc:
                return spuAcc(addr);
        }
        
        printx("PSeudo /// SPU read phase: $%x", (addr & 0xf));
    }
    
    // HW
    switch(addr) {
        case 0x1da6: // Transfer Address
            return spuAddr >> 3;
            
        case 0x1d88: // Sound On 1
        case 0x1d8a: // Sound On 2
        case 0x1d8c: // Sound Off 1
        case 0x1d8e: // Sound Off 2
        case 0x1daa: // Control
        case 0x1dac: // ?
        case 0x1dae: // Status
            return spuAcc(addr);
    }
    
    printx("PSeudo /// SPU read: $%x", addr);
    return 0;
}

void CstrAudio::dataWrite(uw addr, uw size) {
    while(size-- > 0) {
        spuMem.u16[spuAddr >> 1] = accessMem(mem.ram, uh); addr += 2;
        spuAddr += 2;
        spuAddr &= 0x3ffff;
    }
}

void CstrAudio::executeDMA(CstrBus::castDMA *dma) {
    sw size = (dma->bcr >> 16) * (dma->bcr & 0xffff) * 2;
    
    switch(dma->chcr) {
        case 0x01000201: // Write DMA Mem
            dataWrite(dma->madr, size);
            return;
            
//        case 0x01000200:
//            dataMem.read(madr, size);
//            return;
    }
    
    printx("PSeudo /// SPU DMA: $%x", dma->chcr);
}