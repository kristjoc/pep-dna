# RINA config files

1. First, change the names of your ethernet interfaces in the shim templates.

2. Copy these files to /etc/ using the following bash command:

```bash
cp * /etc/
```

## Demo session config file

The Demo session uses a topology as follows:

client - ingress - debian - egress - server

Only the ingress, debian and egress nodes participate in the RINA network.
`demo_configs` dir has the RINA config files for all the nodes. Copy files to
the respective nodes.
