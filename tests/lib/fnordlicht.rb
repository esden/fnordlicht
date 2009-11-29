module Fnordlicht
    def sync(addr = 0)
        1.upto(15) do
            $dev.write "\e"
        end
        $dev.write addr.chr
        $dev.flush
    end

    def stop(addr)
        $dev.write addr.chr
        $dev.write "\x0A"
        $dev.write "\x00\x00\x00\x00\x00"
        $dev.write "\x00\x00\x00\x00\x00"
        $dev.write "\x00\x00\x00"
        $dev.flush
    end

    def fade_rgb(addr, r, g, b, step, delay)
        $dev.write addr.chr
        $dev.write "\x02"
        $dev.write step.chr
        $dev.write delay.chr
        $dev.write r.chr
        $dev.write g.chr
        $dev.write b.chr
        $dev.write "\x00\x00\x00\x00\x00"
        $dev.write "\x00\x00\x00"
        $dev.flush
    end

    def fade_hsv(addr, h, s, v, step, delay)
        $dev.write addr.chr
        $dev.write "\x03"
        $dev.write step.chr
        $dev.write delay.chr
        $dev.write [h].pack('v')
        $dev.write s.chr
        $dev.write v.chr
        $dev.write "\x00\x00\x00\x00\x00"
        $dev.write "\x00\x00"
        $dev.flush
    end

    def modify_current(addr, step, delay, r, g, b, h, s, v)
        $dev.write addr.chr
        $dev.write "\x0b"
        $dev.write step.chr
        $dev.write delay.chr
        $dev.write [r].pack('c')
        $dev.write [g].pack('c')
        $dev.write [b].pack('c')
        $dev.write [h].pack('v')
        $dev.write [s].pack('c')
        $dev.write [v].pack('c')
        $dev.write "\x00\x00\x00\x00"
        $dev.flush
    end

    def config_offsets(addr, step, delay, h, s, v)
        $dev.write addr.chr
        $dev.write "\x07"
        $dev.write [step].pack('c')
        $dev.write [delay].pack('c')
        $dev.write [h].pack('v')
        $dev.write s.chr
        $dev.write v.chr
        $dev.write "\x00\x00\x00\x00\x00\x00\x00"
        $dev.flush
    end

    def config(addr, scripting)
        $dev.write addr.chr
        $dev.write "\x03"
        $dev.write scripting.chr
        $dev.write "\x00\x00\x00\x00\x00\x00\x00"
        $dev.flush
    end

    def fade(addr, speedl, speedh, r, g, b)
        $dev.write addr.chr
        $dev.write "\x02"
        $dev.write speedl.chr
        $dev.write speedh.chr
        $dev.write r.chr
        $dev.write g.chr
        $dev.write b.chr
        $dev.write "\x00\x00\x00"
        $dev.flush
    end

    def start_program(addr, program, params)
        $dev.write addr.chr
        $dev.write "\x09"
        $dev.write program.chr
        rest = 12-params.length
        puts "rest: %u" % rest if $verbose
        params.each do |p|
            $dev.write(p.chr)
        end
        1.upto(rest) do
            $dev.write("\x00")
        end
        $dev.flush
    end

    # secondary functions
    def fade_updown_rgb(addr, r, g, b, step, delay, sleep_time)
        fade_rgb(addr, r, g, b, step, delay)
        sleep(sleep_time)
        fade_rgb(addr, 0, 0, 0, step, delay)
        sleep(sleep_time)
    end

    def fade_updown_hsv(addr, h, s, v, step, delay, sleep_time)
        fade_hsv(addr, h, s, v, step, delay)
        sleep(sleep_time)
        fade_hsv(addr, h, s, 0, step, delay)
        sleep(sleep_time)
    end
end