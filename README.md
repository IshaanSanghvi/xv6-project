# xv6 Demand Paging Implementation

---

## Technologies Used

- C
- RISC-V Assembly
- xv6 Operating System

---

The code in this repository enhances the xv6 operating system with **POSIX-style file-backed memory mapping** and **demand paging**.

This project was motivated by a desire to understand **how lazy page allocation works in modern operating systems**, and how page faults, virtual memory areas, and file-backed mappings interact inside the kernel.

Rather than eagerly loading file data into memory, the system now **allocates and populates pages on first access**, improving performance and memory efficiency for sparse-access workloads.

---

## Approach & Implementation

My approach focused on implementing a minimal but correct version of file-backed virtual memory inside xv6.

I introduced **virtual memory areas** to track active mappings and extended the page-fault handler to dynamically allocate pages and load file data on demand.

For shared writable mappings, modified pages are tracked using the hardware dirty bit and **written back to the underlying file on `munmap()`**.

---

## Performance Evaluation

To evaluate the effectiveness of demand paging, I compared **lazy (demand-paged)** mappings against **eager (default)** mappings.

For each experiment, a file was memory-mapped and a subset of pages was accessed. Timing was measured using kernel ticks.

---

## Results

| Mapping Size | Pages Touched | Lazy Paging | Eager Paging |
|--------------|---------------|-------------|--------------|
| 64 pages (256 KB) | 8 pages | 1 tick | 3 ticks |
| 64 pages (256 KB) | 32 pages | 2 ticks | 3 ticks |

**Observation:** Demand paging achieved up to **3× lower latency** for sparse-access workloads by avoiding unnecessary page-in and disk I/O.

---

## Key Takeaways

- Demand paging significantly reduces startup and access costs when only a subset of pages is used
- Page faults provide a clean and efficient mechanism for on-demand memory allocation
- Lazy allocation trades upfront cost for better average-case performance
