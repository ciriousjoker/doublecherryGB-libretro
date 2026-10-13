#include "clock_math.hpp"
//
// Created by tim on 30.03.26.
//

#include "./gb_core/gb.h"
#include <fstream>
#include <functional>

#include "./DoubleCherryEngine/libretro.h"

extern retro_log_printf_t log_cb;
extern bool logging_transfers_to_file_allowed;

void Infrared_Transceiver::write_RP_REG(byte value, uint32_t now)
{
    RP_Reg new_ir_state;
    new_ir_state.reg_data = value;
    new_ir_state.set_received_signal(rp_reg.received_signal());

    if (rp_reg.ir_light_on() != new_ir_state.ir_light_on())
    {
        Infrared_Signal* sig = new Infrared_Signal(new_ir_state.ir_light_on(), 0);
        sig->start_cycle = now;
        sig->duration    = 0;

        if (!tx.queue.empty())
        {
            Infrared_Signal* prev = tx.queue.front();
            apply_duration_fix(prev, now, new_ir_state);
            int gb_id;
            gb_id = this->ref_gb->get_renderer()->get_gb_id();
            Infrared_Logger::log_ir_traffic_to_file(prev, false, gb_id);
            tx.queue.pop_front();
            delete prev;                                       // FIX #3: kein Leak
        }

        tx.queue.push_back(sig);
        send_edge_ir_signal(sig, now);                             // FIX #1: now weitergeben
    }

    rp_reg.reg_data = new_ir_state.reg_data;
}


byte Infrared_Transceiver::read_RP_REG(uint32_t cycle)
{
    update_rx(cycle);

    if (rp_reg.read_enabled())
    {
        bool level = true;
        if (rx.has_current)
            level = !rx.current->light_on;

        rp_reg.set_received_signal(level);
        return rp_reg.reg_data | 0x3C;
    }
    else
    {
        return rp_reg.reg_data | 0x3E;
    }
}


void Infrared_Transceiver::apply_duration_fix(Infrared_Signal* prev_signal,
                                               uint32_t new_start, RP_Reg next_state)
{
    if (!prev_signal) return;

    const int adjustment = prev_signal->light_on ? RISE_DELAY : LIGHT_FADEOUT_DELAY;
    prev_signal->duration = clock_pulse_duration(new_start, prev_signal->start_cycle, adjustment);
}

void Infrared_Transceiver::receive_edge_ir_signal(Infrared_Signal* new_signal, uint32_t cycle)
{
    // Wir nehmen den cycle EXAKT so wie er vom Sender kommt
    int clocks_diff_between_gbs = ref_gb->get_renderer()->get_gb_id() ? 0 : 4;
    uint32_t new_start = cycle;
    new_signal->start_cycle = new_start + LIGHT_RECOGNITION_DELAY + clocks_diff_between_gbs;
    new_signal->duration    = 0;

    Infrared_Signal* signal_to_correct = nullptr;
    if (!rx.queue.empty()) signal_to_correct = rx.queue.back();
    else if (rx.has_current) signal_to_correct = rx.current;

    if (signal_to_correct && signal_to_correct->duration == 0)
    {
        // Dauer = Jetzt - StartDesLetzten
        signal_to_correct->duration = clock_pulse_duration(new_start,signal_to_correct->start_cycle,LIGHT_FADEOUT_DELAY);

        if (signal_to_correct->duration < 0) signal_to_correct->duration = 0;

        Infrared_Logger::log_ir_traffic_to_file(signal_to_correct, true, ref_gb->get_renderer()->get_gb_id());
    }

    rx.queue.emplace_back(new_signal);
    update_rx(new_start);
}



void Infrared_Transceiver::receive_ir_pulse(Infrared_Signal* new_signal)
{
    rx.queue.emplace_back(new_signal);
}

void Infrared_Transceiver::update_rx(uint32_t now)
{
    if (rx.has_current)
    {
        uint32_t end = rx.current->start_cycle + rx.current->duration;
        bool expired = (rx.current->duration > 0 && clock_at_or_after(now,end));
        bool next_ready = (!rx.queue.empty() && clock_at_or_after(now,rx.queue.front()->start_cycle));

        if (expired || next_ready)
        {
            // HIER KEIN apply_duration_fix MEHR!
            // Das Signal wurde bereits in receive_edge_ir_signal finalisiert.
            delete rx.current;
            rx.current = nullptr;
            rx.has_current = false;
        }
    }

    while (!rx.has_current && !rx.queue.empty())
    {
        Infrared_Signal* next = rx.queue.front();
        if (!clock_at_or_after(now,next->start_cycle)) break;

        rx.queue.pop_front();

        // Falls das Signal beim Abholen schon abgelaufen ist
        if (next->duration > 0 && clock_at_or_after(now,next->start_cycle+uint32_t(next->duration)))
        {
            delete next;
            continue;
        }

        rx.current = next;
        rx.has_current = true;
    }
}


void Infrared_Transceiver::send_edge_ir_signal(Infrared_Signal* signal, uint32_t cycle)
{
    if (infrared_target)
    {
        Infrared_Signal* copy = new Infrared_Signal(*signal);
        infrared_target->receive_edge_ir_signal(copy, cycle);
    }
    else
        log_cb(RETRO_LOG_INFO, "No linked IR target\n");
}


void Infrared_Transceiver::Infrared_Logger::log_ir_traffic_to_file(
    const Infrared_Signal* signal, bool incoming, int gb_id)
{
    if (!logging_transfers_to_file_allowed) return;

    std::ofstream ofs("./ir_logger.txt", std::ios_base::out | std::ios_base::app);
    if (!ofs) return;

    double duration_us = signal->duration * (1'000'000.0 / 4'194'304.0);

    if (incoming)
        ofs << "\t\t\t\t\t\t\t\t\t\t\tgb[" << gb_id  << "]<"
            << signal->light_on << "\t" << signal->duration
            << " (" << duration_us << " us)\n";
    else
        ofs << "gb[" << gb_id  << "]>"
            << signal->light_on << "\t" << signal->duration
            << " (" << duration_us << " us)\n";
}


void Infrared_Transceiver::serialize(serializer& s)
{
    s_VAR(rp_reg.reg_data);
}