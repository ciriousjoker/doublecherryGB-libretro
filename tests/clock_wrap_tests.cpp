#include "gb_core/gb.h"
#include "gb_core/clock_math.hpp"
#include "gb_core/linkcable/include/link_master_device.hpp"
#include "gb_core/linkcable/include/dmg07.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
class TestRenderer final:public renderer {
public:
    void reset() override{} void refresh() override{} void render_screen(byte*,int,int,int) override{}
    int check_pad() override{return 0;} word map_color(word c) override{return c;} word unmap_color(word c) override{return c;}
    byte get_time(int) override{return 0;} void set_time(int,byte) override{} word get_sensor(bool) override{return 0;} void set_bibrate(bool) override{}
};
struct Fixture {
    TestRenderer output;gb game{&output,false,true};std::vector<byte> image=std::vector<byte>(32768,0);cpu *processor;
    explicit Fixture(byte mapper=0){
        image[0x143]=0x80;image[0x147]=mapper;image[0x148]=0;image[0x149]=0;
        check(game.load_rom(image.data(),int(image.size()),nullptr,0,false),"Clock fixture load");processor=game.get_cpu();
        processor->get_regs()->I=0;processor->write(0xff40,0);game.get_regs()->IF=0;
    }
    void step(){processor->get_regs()->PC=0x200;processor->exec(4);}
};
static size_t cpu_size(cpu *p){size_t count=0;serializer s(&count,serializer::COUNT);p->serialize(s);return count;}
static std::vector<byte> save_cpu(cpu *p){std::vector<byte> data(cpu_size(p));serializer s(data.data(),serializer::SAVE_BUF);p->serialize(s);return data;}
static void load_cpu(cpu *p,std::vector<byte>&data){serializer s(data.data(),serializer::LOAD_BUF);p->serialize(s);}
static void cpu_and_serial(){
    for(uint32_t origin:{0x7ffffff0u,0xfffffff0u}){
        Fixture f;f.processor->total_clock=origin;f.processor->seri_occer=0x7fffffffu;
        for(unsigned n=0;n<16;++n)f.step();check(f.processor->total_clock==uint32_t(origin+64),"Defined clock advancement");
        check(!(f.game.get_regs()->IF&INT_SERIAL),"Inactive serial sentinel must not fire");
        int legacy[4]={};f.processor->save_state_ex(legacy);uint32_t bits=0;std::memcpy(&bits,legacy+3,4);check(bits==f.processor->total_clock,"Legacy raw clock bits");
        f.processor->total_clock=0;f.processor->restore_state_ex(legacy);check(f.processor->total_clock==bits,"Legacy raw clock restore");
    }
    for(uint32_t origin:{0x7ffffff0u,0xfffffff0u})for(byte control:{byte(0x81),byte(0x83)}){
        Fixture f;f.processor->total_clock=origin;f.processor->write(0xff01,0x55);f.processor->write(0xff02,control);
        const uint32_t deadline=f.processor->seri_occer;const uint32_t duration=deadline-origin;
        check(duration==(control==0x83?128u:4096u),"CGB transfer period");
        for(unsigned n=0;n<duration/4;++n)f.step();
        check(f.processor->total_clock==deadline&&!(f.game.get_regs()->IF&INT_SERIAL),"Strict equality must not complete early");
        auto pending=save_cpu(f.processor);const size_t expected=8+sizeof(cpu_regs)+32768+16384+128+160+24+16+256*sizeof(dword)+4+20+4+16+1+5;
        check(pending.size()==expected,"CPU serialized width unchanged");
        f.step();check((f.game.get_regs()->IF&INT_SERIAL)&&!(f.game.get_regs()->SC&0x80),"Wrapped transfer completes once");
        f.game.get_regs()->IF=0;f.step();check(!(f.game.get_regs()->IF&INT_SERIAL),"Completed serial becomes inactive");
        load_cpu(f.processor,pending);f.game.get_regs()->IF=0;f.game.get_regs()->SC=control;
        f.step();check(f.game.get_regs()->IF&INT_SERIAL,"Pending wrapped serial restore");
    }
}
static void audio_origin(apu *sound,uint32_t origin){
    size_t size=0;serializer count(&size,serializer::COUNT);sound->serialize(count);
    check(size==2*sizeof(apu_stat)+256+4+2,"APU serialized width unchanged");
    std::vector<byte> data(size);serializer save(data.data(),serializer::SAVE_BUF);sound->serialize(save);
    std::memcpy(data.data()+size-6,&origin,4);serializer load(data.data(),serializer::LOAD_BUF);sound->serialize(load);
}
static std::vector<short> pcm(uint32_t origin){
    Fixture f;auto *sound=f.game.get_apu();sound->reset();audio_origin(sound,origin);f.processor->total_clock=origin;
    const std::array<std::pair<word,byte>,7> setup={{{0xff26,0x80},{0xff24,0x77},{0xff25,0x44},{0xff1a,0x80},{0xff1c,0x20},{0xff1d,0xff},{0xff1e,0x87}}};
    for(unsigned i=0;i<16;++i)sound->write(word(0xff30+i),0xcc,origin);
    for(unsigned i=0;i<setup.size();++i)sound->write(setup[i].first,setup[i].second,origin+4*i);
    f.processor->total_clock=origin+4096;std::vector<short> result(256);sound->get_renderer()->render(result.data(),128);return result;
}
static void audio(){
    const auto normal=pcm(0x10000);check(std::any_of(normal.begin(),normal.end(),[](short v){return v!=0;}),"Non-silent audio fixture");
    auto wrapped=pcm(0x7ffffff0u);
    check(wrapped==normal,"PCM at signed boundary");check(pcm(0xfffffff0u)==normal,"PCM at unsigned rollover");
    Fixture a,b;audio_origin(a.game.get_apu(),0xfffffff0u);audio_origin(b.game.get_apu(),0x100u);
    for(unsigned n=0;n<16;++n){a.game.get_apu()->write(0xff24,0x77,0xfffffff0u+n*4);b.game.get_apu()->write(0xff24,0x77,0x100u+n*4);}
    a.game.get_apu()->reset();a.game.get_apu()->write(0xff24,0x77,0);audio_origin(b.game.get_apu(),0);b.game.get_apu()->write(0xff24,0x77,0);
}
class LinkProbe final:public link_master_device {
public:
    explicit LinkProbe(gb *game){v_gb.push_back(game);}void process() override{}void reset() override{}
    void save_state_mem(void*) override{}void restore_state_mem(void*) override{}void serialize(serializer&s) override{s_VAR(seri_occer);}
    bool ready(){return is_ready_for_next_tik();}
};
static void link_and_ir(){
    // The existing Pikachu WIP class is abstract. Exercise its shared delay logic.
    bool active = false;
    check(!clock_waiting(0, 0, active), "Inactive zero delay");
    active = true;
    check(clock_waiting(0xfffffffcu, 0, active), "Active zero deadline pending");
    check(clock_waiting(0, 0, active), "Delay uses strict equality");
    check(!clock_waiting(4, 0, active), "Active zero deadline expires");
    check(!clock_waiting(0xfffffffcu, 0, active), "Expired delay stays inactive");
    Fixture f;LinkProbe link(&f.game);link.seri_occer=0;
    f.processor->total_clock=0xfffffffcu;check(!link.ready(),"Link deadline pending across wrap");f.processor->total_clock=0;check(link.ready(),"Inclusive link equality");
    dmg07 hub({&f.game});hub.seri_occer=0;f.processor->total_clock=0xfffffffcu;hub.process();f.processor->total_clock=0;hub.process();check(hub.seri_occer!=0,"DMG07 advances wrapped deadline");
    auto *ir=f.game.get_infrared_transceiver();ir->write_RP_REG(0xc0,0);
    auto *pulse=new Infrared_Signal(true,16);pulse->start_cycle=0xfffffff8u;ir->receive_ir_pulse(pulse);
    check(ir->read_RP_REG(0xfffffff4u)&2,"Future IR pulse stays pending");check(!(ir->read_RP_REG(0xfffffff8u)&2),"IR pulse starts at wrapped origin");
    check(!(ir->read_RP_REG(4)&2),"IR pulse remains active after rollover");check(ir->read_RP_REG(8)&2,"IR pulse ends inclusively");
    check(clock_pulse_duration(4,0xfffffffcu)==8&&clock_pulse_duration(0xfffffffcu,4)==0,"Signed IR pulse clamp");
    Fixture huc(0xfe);huc.processor->next_ir_clock=0;huc.processor->next_ir_clock_active=true;huc.processor->total_clock=0xfffffffcu;
    huc.processor->write(0,0x0e);check(!huc.processor->next_ir_clock_active,"HuC3 explicit force-ready");huc.processor->read(0xa000);
}
int main(){try{cpu_and_serial();audio();link_and_ir();std::cout<<"CPU/serial/APU/link/IR clock rollover and unchanged32-bit state pass\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
