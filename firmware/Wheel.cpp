// firmware/Wheel.cpp
#include "Wheel.h"

volatile uint32_t Wheel::edge_buffer_[Wheel::kEdgeBufferSize];
volatile int Wheel::edge_write_index_ = 0;
volatile int Wheel::edge_read_index_ = 0;

Wheel::Wheel() : grouper_(), decoder_(), total_bursts_(0), total_triples_(0) {}

void Wheel::begin() {
  // Without this, micros() falls back to the ~977us RTOS tick, which is far
  // too coarse for BurstGrouper's 20us burst gap. With it, micros() reads the
  // CPU cycle counter (1us resolution at 64 MHz).
  dwt_enable();
  pinMode(kPulsePin, INPUT);
  attachInterrupt(digitalPinToInterrupt(kPulsePin), handleInterrupt, RISING);
}

void Wheel::handleInterrupt() {
  int next = (edge_write_index_ + 1) % kEdgeBufferSize;
  if (next == edge_read_index_) return;  // buffer full -- drop rather than overwrite
  edge_buffer_[edge_write_index_] = micros();
  edge_write_index_ = next;
}

WheelPulseDecoder::Direction Wheel::update() {
  while (edge_read_index_ != edge_write_index_) {
    uint32_t timestamp = edge_buffer_[edge_read_index_];
    edge_read_index_ = (edge_read_index_ + 1) % kEdgeBufferSize;

    int completed_size;
    if (grouper_.pulse(timestamp, &completed_size)) {
      total_bursts_++;
      if (completed_size >= 3) total_triples_++;
      decoder_.update(completed_size);
    }
  }
  return decoder_.current();
}
