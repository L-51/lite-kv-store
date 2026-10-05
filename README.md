# Lite KV-Store (In-Memory Key-Value Database)

A lightweight, extremely fast in-memory Key-Value store written in modern C++.

## Features
- **O(1) Time Complexity**: Built on top of `std::unordered_map` for fast lookups and insertions.
- **Thread-Safe**: Uses `std::shared_mutex` to allow highly concurrent `GET` operations while keeping `SET` operations completely safe and atomic.
- **CLI Interface**: Includes a lightweight Read-Eval-Print Loop for immediate database interaction.

## Motivation
This project was built to demonstrate a deep understanding of memory management, data structures, and multithreading concurrency in C++, principles essential for high-performance Systems Engineering and backend infrastructure.
