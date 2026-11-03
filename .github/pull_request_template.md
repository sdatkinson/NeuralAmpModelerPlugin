**Thanks for making a Pull Request!**
Please fill out this template so that you can be sure that your PR does everything it needs to be accepted.

## Description
What does your PR do?
Include [Closing words](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue) to link this PR to the Issue(s) that it relates to.

## PR Checklist
- [ ] Did you format your code using [`format.bash`](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/format.bash)?
- [ ] Does your PR add, remove, or rename any plugin parameters? If yes...
  - [ ] Have you ensured that the plug-in unserializes correctly?
  - [ ] Have you ensured that _older_ versions of the plug-in load correctly? (See [`Unserialization.cpp`](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/NeuralAmpModeler/Unserialization.cpp).)
- [ ] Does your PR add or remove any graphical assets? If yes, are they defined in [config.h](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/NeuralAmpModeler/config.h) and added in the two required locations in [main.rc](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/NeuralAmpModeler/resources/main.rc)?
  
