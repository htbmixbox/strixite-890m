#include "serve/server_config.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace strix;

int main() {
    const auto opts = strix_server_options("/tmp/strix-models");
    ServerSettings defaults(opts);
    assert(defaults.integer("capacity") == 65536);
    assert(defaults.integer("chunk") == 2048);
    assert(defaults.integer("ngram-cache-rows") == 262144);
    assert(defaults.integer("prompt-cache-ram-margin-gib") == 6);
    assert(defaults.number("rope-yarn-factor") == 1.0);
    assert(defaults.integer("prompt-cache-gib") == 128);
    assert(defaults.number("prompt-cache-idle-s") == 600);
    assert(defaults.number("prompt-cache-write-gib-per-hour") == 0);
    assert(defaults.integer("prompt-cache-checkpoints") == 3);
    assert(defaults.integer("prompt-cache-delta-max-tokens") == 16384);
    assert(defaults.text("low-memory") == "fail");

    ServerSettings configured(opts);
    configured.load({"--config", "deploy/890m-96gb.conf"});
    for (const auto &o : opts) {
        // Exempt paths: set from models dir; also keep every configuration knob
        // not present in the file at its (intended) compiled-in default.
        assert(configured.setting(o.name).value == defaults.setting(o.name).value);
    }

    ServerSettings override(opts);
    override.load({"--config", "deploy/890m-96gb.conf", "--capacity", "32768",
                   "--chunk", "1024", "--mtp", "off", "--print-config"});
    assert(override.print_config());
    assert(override.integer("capacity") == 32768);
    assert(override.integer("chunk") == 1024);
    assert(!override.on("mtp"));
    assert(override.setting("capacity").source == "flag");
    assert(override.render().find("capacity = 32768") != std::string::npos);

    ServerSettings halo(opts);
    halo.load({"--config", "deploy/strix-server.conf"});
    assert(halo.integer("capacity") == 524288);
    assert(halo.integer("chunk") == 16384);
    assert(halo.integer("ngram-cache-rows") == 8388608);
    assert(halo.number("rope-yarn-factor") == 2.0);

    std::cout << "PASS: 890M built-in settings, file equality, overrides and Halo compatibility\n";
}
