use std::{net::Ipv4Addr, ops::Range, str::FromStr, time::Duration};

use anyhow::Context;
use pistol::{scan::PortStatus, tcp_syn_scan, Host, Target};

pub fn syn_scan(dst_ipv4: &str, port_range: Range<u16>) -> anyhow::Result<Vec<u16>> {
    let src_ipv4 = None;
    // If the value of `source port` is `None`, the program will generate the source port randomly.
    let src_port = None;
    // The destination address is required.
    let dst_ipv4 = Ipv4Addr::from_str(dst_ipv4)?;
    let threads_num = 4;
    let timeout = Some(Duration::new(1, 0));
    let host = Host::new(dst_ipv4.into(), Some(port_range.collect()));
    // Users should build the `target` themselves.
    let target = Target::new(vec![host]);
    // Number of tests
    let tests = 1;
    let ret = tcp_syn_scan(target, src_ipv4, src_port, threads_num, timeout, tests)?;

    let open_ports = ret
        .scans
        .get(&dst_ipv4.into())
        .context("Can't get destination IP")?
        .iter()
        .filter_map(|(port, status)| {
            if status.contains(&PortStatus::Open) {
                Some(port.to_owned())
            } else {
                None
            }
        })
        .collect::<Vec<u16>>();
    Ok(open_ports)
}
