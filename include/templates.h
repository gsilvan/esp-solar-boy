//
// Created by silvan on 17.01.25.
//
#include <Arduino.h>

#ifndef ESP_SOLAR_BOY_TEMPLATES_H
#define ESP_SOLAR_BOY_TEMPLATES_H

static const char indexHtmlTemplate[] PROGMEM =
        R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:ital,wght@0,100;0,200;0,300;0,400;0,500;0,600;0,700;1,100;1,200;1,300;1,400;1,500;1,600;1,700&display=swap" rel="stylesheet">
    <title>Solar-Boy-2000</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body { font-family: "IBM Plex Mono", monospace; }
        .pin-active { background-color: green; }
        .pin-inactive { background-color: gray; }
    </style>
</head>
<body class="bg-gray-100">
    <div class="flex h-screen overflow-hidden">
        <!-- Sidebar Navigation -->
        <nav id="sidebar" class="w-64 bg-white shadow-lg transform -translate-x-full transition-transform duration-300 ease-in-out lg:translate-x-0 fixed inset-y-0 left-0 z-30 lg:relative">
            <div class="p-6">
                <h1 class="text-2xl font-bold text-gray-800">SolarBoy 300</h1>
            </div>
            <ul class="mt-6">
                <li>
                    <a href="/" class="flex items-center px-6 py-3 text-gray-700 hover:bg-gray-100 hover:text-blue-600">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6" />
                        </svg>
                        Dashboard
                    </a>
                </li>
                <li>
                    <a href="/settings" class="flex items-center px-6 py-3 text-gray-700 hover:bg-gray-100 hover:text-blue-600">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                        </svg>
                        Settings
                    </a>
                </li>
                <li>
                    <a href="/update" class="flex items-center px-6 py-3 text-gray-700 hover:bg-gray-100 hover:text-blue-600">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M16 8v8m-4-5v5m-4-2v2m-2 4h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
                        </svg>
                        Firmware
                    </a>
                </li>
                <li><a class="flex items-center px-6 py-3 text-gray-700 text-xs" href="https://github.com/gsilvan/esp-solar-boy/releases">%VERSION%</a></li>
            </ul>
        </nav>

        <!-- Main Content Area -->
        <main class="flex-1 overflow-x-hidden overflow-y-auto">
            <header class="bg-white shadow-md p-4 flex items-center justify-between lg:hidden">
                <h1 class="text-xl font-semibold text-gray-800">SolarBoy 300</h1>
                <button id="menuToggle" class="text-gray-500 hover:text-gray-600 focus:outline-none focus:text-gray-600" aria-label="Toggle menu">
                    <svg class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16" />
                    </svg>
                </button>
            </header>
            <div class="p-6">
                <h2 class="text-3xl font-semibold text-gray-800 mb-6">Dashboard</h2>
                <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
                    <!-- Sample Card 1 -->
                    <div class="bg-white rounded-lg shadow-md p-6">
                        <h3 class="text-xl font-semibold text-gray-700 mr-3">Pin state</h3>
                        <p class="text-3xl font-bold text-blue-600">
                            <div class="w-4 h-4 rounded-full %PIN_0_CLASS%"></div>
                        </p>
                    </div>
                    <!-- Sample Card 2 -->
                    <div class="bg-white rounded-lg shadow-md p-6">
                        <h3 class="text-xl font-semibold text-gray-700 mb-2">Battery charge</h3>
                        <p class="text-3xl font-bold text-green-600">%BATTERYCHARGE% %</p>
                    </div>
                    <!-- Sample Card 3 -->
                    <div class="bg-white rounded-lg shadow-md p-6">
                        <h3 class="text-xl font-semibold text-gray-700 mb-2">Charge</h3>
                        <p class="text-3xl font-bold text-yellow-600">%CHARGE% W</p>
                    </div>
                    <!-- Sample Card 4 -->
                    <div class="bg-white rounded-lg shadow-md p-6">
                        <h3 class="text-xl font-semibold text-gray-700 mb-2">Input power</h3>
                        <p class="text-3xl font-bold text-yellow-600">%INPUTPOWER% W</p>
                    </div>
                </div>
                <div class="py-3">
                    <p class="text-sm text-gray-500 mt-2">Last updated: <span class="timestamp">%UNIXTIMESTAMP%</span></p>
                </div>
            </div>
        </main>
    </div>

    <script>
        const menuToggle = document.getElementById('menuToggle');
        const sidebar = document.getElementById('sidebar');

        menuToggle.addEventListener('click', () => {
            sidebar.classList.toggle('-translate-x-full');
        });

        // Close sidebar when clicking outside of it
        document.addEventListener('click', (e) => {
            if (!sidebar.contains(e.target) && !menuToggle.contains(e.target)) {
                sidebar.classList.add('-translate-x-full');
            }
        });

        document.addEventListener("DOMContentLoaded", () => {
            const timestamps = document.querySelectorAll(".timestamp");
            timestamps.forEach(element => {
                const unixTimestamp = parseInt(element.textContent, 10);
                if (!isNaN(unixTimestamp)) {
                    const date = new Date(unixTimestamp * 1000);
                    const localizedDate = date.toLocaleString();
                    element.textContent = localizedDate;
                } else {
                    console.error("Invalid timestamp: ", element.textContent);
                }
            });
        });
    </script>
</body>
</html>
)";

static const char settingsHtmlTemplate[] PROGMEM =
        R"(
<!DOCTYPE html>
<html>
<head>
    <title>Solar-Boy-2000</title>
    <style>
        .input { width: 100%; }
    </style>
</head>
<body>
    <h1>Settings</h1>
    <form method="post">
    <div style="max-width: 450px;">
        <h2>Device</h2>
        <label for="settings-ssid">SSID:</label>
        <input class="input" type="text" id="settings-ssid" name="settings-ssid" placeholder="SSID">
        <label for="settings-password">Password:</label>
        <input class="input" type="password" id="settings-password" name="settings-password" placeholder="Password">
        <label for="settings-inverter-ip">Inverter IPv4:</label>
        <input class="input" type="text" id="settings-inverter-ip" name="settings-inverter-ip" value="%IPADDRESS%">
        <button>Save</button>
        <h2>PINs</h2>
        <h3>PIN_0</h3>
        <label for="pin-0-battery">Battery charge (%):</label>
        <input class="input" type="number" min="0" max="100" step="1" id="pin-0-battery" name="pin-0-battery" value="%BATTERYCHARGE%">
        <label for="pin-0-input-power">Power overflow (Watts):</label>
        <input class="input" type="number" min="100" step="100" id="pin-0-input-power" name="pin-0-input-power" value="%PIN0INPUTPOWER%">
        <label for="pin-0-timer">Monitoring window (minutes):</label>
        <input class="input" type="number" min="0" max="60" step="1" id="pin-0-timer" name="pin-0-timer" value="%PIN0TIMER%">
        <label for="pin-0-cycle">Switch cycle (minutes):</label>
        <input class="input" type="number" min="0" max="60" step="1" id="pin-0-cycle" name="pin-0-cycle" value="%PIN0CYCLE%">
    </div>
    <button>Save</button>
    </form>
    <a href="/">back</a>
</body>
</html>
)";

#endif //ESP_SOLAR_BOY_TEMPLATES_H
