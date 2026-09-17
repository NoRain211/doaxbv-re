# Current local generation recipe

`recipe.json` authenticates the lifter patch and every generation input. The
builder checks this recipe hash before using it and checks every generated
file against the current proven local program before compilation.

The function shards contain addresses and analysis metadata. The 45 recovery
files must retain their listed order. Neither contains machine instructions or
game assets. Manual call targets preserve native adapter routing.

The final recovery input restores the input-history owner and the reached Radio
Station callback. These bodies match the locally tested program;
the expiry correction keeps comparisons and their branches in the same body.

The Rest recoveries supply a missing callback and its two dependencies, and join
each of two reached predicates' true/false returns into one owner. Other generated
bodies remain unchanged. The corrected Take a Rest variants still need local testing.

The setup suite generates synthetic expiry logic with this recovery and checks
that the input-history comparisons and branches share one body, and that the
Radio callback retains its full range. It also exercises both local returns of
both Rest predicates. It requires no game files. Run:

```sh
git submodule update --init tools/xboxrecomp
python -m pip install capstone==5.0.9
python -m unittest discover -s tools -p 'test_*.py'
```

The source base is upstream `32da23872a552b12b4a932c9d5a6e952bb3f24bb`.
Apply `local-parity.patch` directly to that clean revision. Do not first apply
`runtime-bootstrap.patch`. The patch reconstructs the preserved working local
lifter, including the explicit x87 destination correction used by camera
calculations; it is not an upgrade to the latest upstream release.

Changing the recipe requires regenerating from the supported user-owned XBE,
reviewing output differences, and validating the affected player-visible flows.
Do not update expected hashes merely to make a mismatched build pass.
