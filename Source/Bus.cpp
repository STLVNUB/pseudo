#import "Global.h"


CstrBus bus;

void CstrBus::checkDMA(uw addr, uw data) {
    ub chan = ((addr >> 4) & 0xf) - 8;
    
    if (dpcr & (8 << (chan * 4))) { // GPU does not execute sometimes
        castDMA *dma = (castDMA *)&mem.hwr.ptr[addr & 0xfff0];
        dma->chcr = data;
        
        switch(chan) {
            case DMA_GPU:
                vs.executeDMA(dma);
                break;
                
            case DMA_SPU: // TODO
                audio.executeDMA(dma);
                break;
                
            case DMA_CLEAR_OT:
                mem.executeDMA(dma);
                break;
                
            default:
                printx("PSeudo /// DMA Channel: %d", chan);
                break;
        }
        dma->chcr = data & ~0x01000000;
        
        if (dicr & (1 << (16 + chan))) {
            dicr |= 1 << (24 + chan);
            data16 |= 8;
        }
    }
}
