/**
 * Network functionalities are vital for namespaces.
 * Thanks by network namespaces, you can have different and separate
 * instances of network interfaces and routing table that operate
 * independent of each other.
 * 
 * Here's what we will need to do:
 *      1 - Create a bridge device in the host's Network namespace
 *      2 - Create a veth pair
 *      3 - Attach one side of the pair to the bridge
 *      4 - Place the other side of the pair in our container network
 *          namespace
 *      5 - Ensure all traffic originating in the namespaces process
 *          gets routed via the veth
 * 
 * The general idea is to establish a connection between the process of
 * our Network namespace and the host's Network namespace.
 * 
 *        Host Network namespace                Our process Network Namespace
 * +----------------------------------+     +------------------------------------+
 * |                                  |     |                                    |
 * |    +-----------------------+     |     |                                    |
 * |    |          lo           |     |     |                                    |
 * |    +-----------------------+     |     |                                    |
 * |                                  |     |                                    |
 * |    +-----------------------+     |     |                                    |
 * |    |         eth0          |     |     |                                    |
 * |    +-----------------------+     |     |                                    |
 * |                                  |     |                                    |
 * |    +-----------------------+     |     |      +-----------------------+     |
 * |    |         brg0          |     |     |      |           lo          |     |
 * |    +--|--------------------+     |     |      +-----------------------+     |
 * |       |                          |     |                                    |
 * |    +--|--------------------+     |     |      +-----------------------+     |
 * |    |         veth0       --+-----+-----+------+--        veth1        |     |
 * |    +-----------------------+     |     |      +-----------------------+     |
 * |                                  |     |                                    |
 * +----------------------------------+     +------------------------------------+
 * 
 * A virtual network (veth) device pair provides a pipe-like abstraction
 * that can be used to create tunnels between network namespaces, and
 * can be used to create a bridge to a physical network device in
 * another namespace. When a namespace is freed, the veth(4) devices
 * that it contains are destroyed.
 * 
 * Network setup requires root privileges but we want run our network
 * namespace process as non-root user. (see user_namespace unser
 * /src/namespaces/user). Fortunately this can be avoided by making
 * use of setuid. Setuid allows a process to run as the user that 'owns'
 * an executable. The idea is to extract the network setup code into a
 * separate executable. Ensure the executable is owned by the root user
 * and to apply the setuid permision on it.
 * We can then call out to the executable from within our process (running
 * as non-root user) as and when we need to.
 */
void start_network(pid_t child_pid);