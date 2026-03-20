## Summary

- 

## Related Issue

- Closes #

## Area

- [ ] Engine
- [ ] Domain
- [ ] Application
- [ ] Docs
- [ ] Build
- [ ] Analysis
- [ ] Chore

## Architecture Check

- [ ] I kept the dependency direction `application -> domain -> engine`.
- [ ] I did not add Qt UI code to `src/domain`.
- [ ] I did not add `domain` or `application` dependencies to `src/engine`.
- [ ] I used `src/` as the include root.

## Verification

- [ ] `cmake --preset windows-debug`
- [ ] `cmake --build --preset build-debug`
- [ ] Not run (reason below)

## Risks / Follow-up

- 
