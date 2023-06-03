/* DO NOT INCLUDE IN PATCH - X86-ONLY AND REQUIRES C++20. */
/* Timer with labeled checkpoints. */

#ifndef GDBSUPPORT_CHECKPOINT_TIMER_H
#define GDBSUPPORT_CHECKPOINT_TIMER_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <x86intrin.h>

namespace gdb
{

class checkpoint_timer
{
private:
  enum class event_type { begin, end };

  std::vector<std::string> labels;
  std::vector<uint64_t> times;
  std::vector<event_type> events;

  size_t depth = 0;
  size_t max_depth = 0;

public:
  checkpoint_timer ()
  {}

  void begin (std::string name)
  {
    depth += 1;
    if (max_depth < depth)
      max_depth = depth;

    events.push_back (event_type::begin);
    times.push_back (__rdtsc ());
    labels.push_back (name);
  }

  void end ()
  {
    if (depth == 0)
      throw std::runtime_error ("awooooooooo");
    depth -= 1;

    events.push_back (event_type::end);
    times.push_back (__rdtsc ());
  }

  void dump () const
  {
    std::vector<std::tuple<std::string, uint64_t>> stack;
    stack.reserve (max_depth);

    size_t label_i = 0;
    size_t depth = 0;

    const auto println_with_indent = [&depth](std::string msg)
    {
      for (size_t i = 0; i < depth; ++i)
        printf ("|   ");
      printf("%s\n", msg.c_str ());
    };

    for (size_t i = 0; i < events.size (); ++i)
    {
      const auto event = events[i];
      if (event == event_type::begin)
        {
          stack.push_back (std::make_tuple (labels[label_i], times[i]));
          println_with_indent (labels[label_i++] + ":");
          depth += 1;
        }
      else if (event == event_type::end)
        {
          depth -= 1;
          const auto [label, beg_time] = stack.back ();
          const auto end_time = times[i];

          println_with_indent ("end time: " + std::to_string (end_time - beg_time) + "cycles");

          stack.pop_back ();
        }
    }
  }
};

#endif

}