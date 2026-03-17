# AuralReality Protocol

This repository contains protocol buffer/gRPC definitions for the
communications protocol of the [AuralReality
project](https://www.hlrs.de/projects/detail/auralreality).

The protocol is designed for synchronization of audio-related objects between
the **sound design and authoring tool** and the **virtual reality
environment**. It contains data types and procedures to modify objects such as
virtual sound sources, speakers in the virtual environment, trajectories for
moving virtual souund sources and other data, as supported by the authoring
tool.

For real-time playback and integration in the virtual environment, VR tracking
data and the virtual world navigation is also transmitted via this protocol.
