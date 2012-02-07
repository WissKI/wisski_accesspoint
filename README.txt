
wisski_accesspoint
=============================

This module provides a SPARQL endpoint interface to diverse data pools. It is
used by the module wisski_vocab_ctrl as source for the vocabulary's data.

installation instructions and setup
=============================

1. copy the files to Drupal's sites/all/modules.
2. Activate the module in the admin settings.

You can then define accesspoints via Site Configuration ->
Wisski Module settings -> Access Points.
The module provides the following types of endpoints:
1. The local triple store. This is always present nd cannot be altered.
2. 'SparQL Endpoint': provides access to any sparql endpoint that supports
SPROT (see <http://semanticweb.org/wiki/SPROT>). You only have to specify the
URL of the endpoint. Optionally you can enter additional parameters for the
HTTP GET request.
3. 'Inter-WissKI': contact the triple store of other WissKI instances. You can
provide a key for access-restricted WissKI stores (see module wisski_endpoint).
4. 'Store': Set up a separate local triple store which you can use as separate
data pool. After init, you may import triple data. This type is only present if
module wisski_ap_store is enabled.

