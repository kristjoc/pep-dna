<!-- PROJECT SHIELDS -->

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]

<!-- PROJECT LOGO -->
<br />
<p align="center">
  <h3 align="center">PEP-DNA</h3>

  <p align="center">
    PEP-DNA: a Performance Enhancing Proxy for Deploying Network Architectures
    <br />
    <a href="https://github.com/kr1stj0n/pep-dna/wiki"><strong>Explore the Wiki »</strong></a>
    <br />
    <br />
    <a href="https://github.com/kr1stj0n/pep-dna/wiki/Tutorials">View Tutorials</a>
    ·
    <a href="https://github.com/kr1stj0n/pep-dna/issues">Report Bug</a>
    ·
    <a href="https://github.com/kr1stj0n/pep-dna/issues">Request Feature</a>
  </p>
</p>


<!-- TABLE OF CONTENTS -->
<details open="open">
        <summary><h2 style="display: inline-block">Table of Contents</h2></summary>
        <ol>
        <li><a href="#introduction">Introduction</a></li>
        <li><a href="#build-instructions">Build instructions</a>
        <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
        </ul>
        </li>
        <li><a href="#reproducibility">Reproducibility</a></li>
        <li><a href="#contributing">Contributing</a></li>
        <li><a href="#license">License</a></li>
        <li><a href="#contact">Contact</a></li>
        <li><a href="#acknowledgements">Acknowledgements</a></li>
  </ol>
</details>


<!-- INTRODUCTION -->
## Introduction

PEP-DNA is a Performance Enhancing Proxy designed specifically for Deploying new
Network Architectures. It is implemented in the Linux kernel and can be
installed wherever a translation needs to occur between a new architecture and
TCP/IP domains. PEP-DNA is currently able to interconnect a TCP connection with
(i) another TCP connection, (ii) the Recursive InterNetwork Architecture (Visit
<http://pouzinsociety.org> to read more about RINA), and (iii) a Content-Centric
Networking domain
(<https://named-data.net/publications/van-ccn-bremen-description/>). This
`README.md` file provides information on repeating and replicating the results
of the paper. For more information on how to use PEP-DNA in different scenarios,
check the Tutorials on the <a
href="https://github.com/kr1stj0n/pep-dna/wiki">Wiki</a> pages.

<!-- BUILD INSTRUCTIONS -->
## Build instructions

We have tested PEP-DNA with the current stable distribution of Ubuntu and
Debian, with kernel versions between 4.1.x to 5.4.x. In order to run all the
experiments described in the paper, PEP-DNA needs to be built with RINA and CCN
support. We use the RINA stack, which is available at
<https://github.com/IRATI/stack>, and CCN-lite, which can be found at
<https://github.com/cn-uofbasel/ccn-lite>, to build RINA and CCN network. To
facilitate the installation process, we include the RINA stack and CCN-lite in
this repository.

**Note** that a user with sudo privileges is required to load PEP-DNA and RINA
kernel modules apply other commands.

### Prerequisites

First, install the user-space dependencies which are required to build RINA.
   ```sh
   sudo apt-get update
   sudo apt-get install build-essential autoconf automake libtool pkg-config git
   g++ libssl-dev libelf-dev protobuf-compiler libprotobuf-dev socat python python3
   openjdk-11-jre openjdk-11-jdk linux-headers-$(uname -r)
   ```
Next, use the following to install the requirements for CCN-lite:
   ```sh
   sudo apt-get install cmake libssl-dev pkg-config libssl-dev libcmocka-dev
   ```
Install libnl-3-dev for Netlink sockets support.
   ```sh
   sudo apt-get install libnl-3-dev
   ```
Additional tools are required to configure Linux hosts, run the experiments and
collect information.
   ```sh
   sudo apt-get install sysstat ethtool cpufrequtils httping httperf apache2
   ```

### Installation

1. Clone the repository. We recommend to clone the repo in the home directory so
that it matches the path used in the scripts.

```bash
  cd ~
  git clone https://github.com//pep-dna.git
```

2. Build and install RINA's kernel-space and user-space software
   ```bash
   cd ~/pep-dna/rina-stack
   sudo ./configure
   sudo make install
   ```
3. Build CCN-lite
    - Set environment variable `$CCNL_HOME` and add the binary folder of CCN-lite to your `$PATH`:
    ```bash
    cd ~/pep-dna/ccn-lite
    export CCNL_HOME="`pwd`"
    export PATH=$PATH:"$CCNL_HOME/bin"
    ```
    To make these variables permanent, add them to your shell's `.rc` file, e.g. `~/.bashrc`.

    - Build CCN-lite using `cmake`:
    ```bash
    cd $CCNL_HOME
    mkdir build && cd build
    cmake ..
    make
    ```
4. Configure and install PEP-DNA
   ```bash
   cd ~/pep-dna/pepdna
   ./configure --with-rina --with-ccn --with-localhost
   make all
   sudo -E make install
   ```

To configure PEP-DNA with RINA support or CCN suport `--with-rina` or `--with-ccn` flags needs to be used. Use `--with-debug` to build PEP-DNA with DEBUG flag.
   **Note** that building in debug mode will reduce the performance of the proxy and print detailed logging in the kern.log file. When PEP-DNA runs at the same host as the server, it needs to be configured with `---with-localhost` flag in order to enable full transpacency at this case (More details will be provided later). For our experiments, the commands above are sufficient.
4. All the testing applications and scripts used to run the experiments, collect the results and plot the graphs are located in https://github.com/kr1stj0n/pep-dna/tree/main/pepdna/utils/apps. Run the following commands to install them to `/usr/bin/`.
   ```bash
   cd ~/pep-dna/pepdna/utils/apps
   make all
   sudo -E make install
   ```

<!-- USAGE EXAMPLES -->
## Reproducibility

We aim to make our work entirely reproducible and encourage interested researchers to test the code and replicate the reported experimental results. The PEP-DNA implementation and documentation needed to reproduce all the experiments described in the paper are available in this public repository. The tools we developed to the run the experiments were installed at Step 4 of the previous section. The scripts for automated testing, analysis and plotting of the generated data are located at https://github.com/kr1stj0n/pep-dna/tree/main/pepdna/utils/scripts/ alongside with a detailed `README_scripts.md` file.
Please, read the `README_scripts.md` file for a detailed explanation on how to set the variables for your own testbed environment, run all the experiments and plot the generated dataset.

_For more step-by-step examples on how to use PEP-DNA in different scenarios, please refer to the <a href="https://github.com/kr1stj0n/pep-dna/wiki">Wiki</a> pages._


<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/udp-support`)
3. Commit your Changes (`git commit -m 'Added UDP support'`)
4. Push to the Branch (`git push origin feature/udp-support`)
5. Open a Pull Request


<!-- LICENSE -->
## License

Distributed under the GPL License. See `LICENSE` in https://github.com/kr1stj0n/pep-dna/tree/main/pepdna/ for more information.


<!-- CONTACT -->
## Contact

Kristjon Ciko - kristjoc@ifi.uio.no

Project Link: [https://github.com/kr1stj0n/pep-dna](https://github.com/kr1stj0n/pep-dna)


<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements

* [RINA implementation for OS/Linux](https://github.com/IRATI/stack)
* [CCN-lite: a lightweight implementation of the CCNx protocol and its variations](https://github.com/cn-uofbasel/ccn-lite)


<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/kr1stj0n/pep-dna.svg?style=for-the-badge
[contributors-url]: https://github.com/kr1stj0n/pep-dna/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/kr1stj0n/pep-dna.svg?style=for-the-badge
[forks-url]: https://github.com/kr1stj0n/pep-dna/network/members
[stars-shield]: https://img.shields.io/github/stars/kr1stj0n/pep-dna.svg?style=for-the-badge
[stars-url]: https://github.com/kr1stj0n/pep-dna/stargazers
[issues-shield]: https://img.shields.io/github/issues/kr1stj0n/pep-dna.svg?style=for-the-badge
[issues-url]: https://github.com/kr1stj0n/pep-dna/issues
