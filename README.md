# WiiLink WFC DNS Proxy
Local proxy server for WiiLink WFC users whose ISPs block using a custom DNS. Run this tool on your computer and then enter your computer's IP address into the DNS settings of your Wii or Nintendo DS console. More detailed instructions are provided upon running the program.

## Usage
Simply run the program (from the command line) and follow the provided instructions. On Windows, you may need to allow the program through Windows Firewall. On Linux, you may need to run the program with `sudo` to get proper permission to bind on the port. Other than that, most error messages are pretty self-explanatory.

## Compiling
You can build the program by simply cloning the repository and running `make` in the root. This will create an output file `wwfc-dns-proxy` (`wwfc-dns-proxy.exe` on Windows). The Makefile expects `gcc` to provided in PATH and/or accessible from the current working directory.

## How it works
Some ISPs block connecting to custom DNS servers by redirecting all traffic on port 53 (the standard DNS port) to their own DNS server. We can get around this by adding support for a non-standard port on the server, however, most devices (including the Wii and DS) don't allow specifying a custom port to use for the DNS, enforcing the standard port 53.

While connecting to custom public-facing DNS server may be impossible with this restriction, you *are* able to connect to a DNS server on the local network, as that connection only goes through your router and local network, and does not reach your ISP. Utilizing this fact, we can host a local server on the standard DNS port that simply forwards packets to and from the remote DNS server on a non-standard port, creating a bridge between your device and the WiiLink WFC DNS server.
