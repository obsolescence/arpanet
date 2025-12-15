echo "ping 52->53:"
NCP=ncp52 ./ncp-ping -c1 053
echo
echo "ping 53->52:"
NCP=ncp53 ./ncp-ping -c1 052
echo
echo "ping 54->52, through 53:"
NCP=ncp54 ./ncp-ping -c1 052
echo
echo "ping 52->54, through 53:"
NCP=ncp52 ./ncp-ping -c1 054
echo
echo "ping 62->52, through 53/54:"
NCP=ncp62 ./ncp-ping -c1 052
echo
echo "ping 52->62, through 53/54:"
NCP=ncp52 ./ncp-ping -c1 62
echo
echo "ping 52->62;1, to its through 53/54:"
NCP=ncp52 ./ncp-ping -c1 126
