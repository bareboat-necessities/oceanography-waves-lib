#pragma once

void test_csv_timestamps() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wave-clock-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{directory};
    for (int precision : {6, 9}) {
        const auto filename = (directory / "clock.csv").string();
        WaveDataCSVWriter writer(filename, false, precision);
        writer.write_header();
        // Full production duration, including 100 and 1000 second boundaries.
        for (int step = 0; step <= 240000; ++step) {
            Wave_Data_Sample sample{};
            sample.time = step / 200.0;
            sample.wave.disp_x = 1234.5678f;
            writer.write(sample);
        }
        writer.close();
        int count = 0;
        WaveDataCSVReader reader(filename);
        reader.for_each_record([&](const Wave_Data_Sample& sample) {
            require(sample.time == count / 200.0, "CSV must round-trip each 200 Hz timestamp exactly");
            ++count;
        });
        require(count == 240001, "CSV must retain all samples through 1200 seconds");
        // Arbitrary double timestamps and append mode must also be lossless.
        Wave_Data_Sample sample{};
        sample.time = std::nextafter(1234567.123456789, 0.0);
        WaveDataCSVWriter append(filename, true, precision);
        append.write(sample);
        append.close();
        std::ifstream raw(filename);
        std::string line, last;
        while (std::getline(raw, line)) last = line;
        require(std::stod(last.substr(0, last.find(','))) == sample.time,
                "Timestamp precision must be independent of state precision and time magnitude");
    }
}

void test_fenton_sample_clock() {
    WaveSurfaceTracker<5, double> tracker(0.27, 200.0, 14.047, 0.0, 5.0, 0.1);
    int count = 0;
    tracker.track_floating_object(1200.0, 0.005,
        [&](double t, double dt, double z, double vz, double az, double x, double vx) {
            require(t == (count + 2) / 200.0, "Fenton time must use the exact sample index");
            require(dt == 0.005, "Fenton integration interval must match the sample clock");
            for (double value : {z, vz, az, x, vx}) require_finite(value, "Fenton state must be finite");
            ++count;
        });
    require(count == 239999, "Fenton must preserve startup and include the 1200 second endpoint");
}
