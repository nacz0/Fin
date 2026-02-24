# Fin Agent Notes

## Szybkie polecenia
- Build (Debug): `cmake --build build-fastener --config Debug -- /m:1`
- Uruchom aplikacje: `.\build-fastener\Debug\Fin.exe`
- Szukanie w kodzie: `rg -n "wzorzec" src`

## Czeste bledy (i jak ich nie powtarzac)
- **Wyciek inputu do wielu widgetow**  
  `TextEditor` nie moze czytac klawiatury globalnie. Klawiatura ma dzialac **tylko** gdy edytor ma focus (`handleWidgetInteraction` + `getWidgetState(...).focused`).
- **Brak nowej linii w terminalu na Windows**  
  Do `cmd.exe` wysylaj `\r\n`, nie samo `\n`.
- **Ucinanie terminala przez sztywne wymiary**  
  Nie ustawiaj szerokosci inputu/obszaru "na stale". Uzywaj szerokosci z aktualnych bounds panelu.
- **Input terminala poza terminalem**  
  Nie rozdzielaj wpisywania do osobnego paska "pod spodem", jesli wymagane jest pisanie "w terminalu". Input ma byc renderowany jako ostatnia linia wewnatrz obszaru terminala.
- **Edytowalna historia terminala**  
  Historia wyjscia ma byc `readonly`. Kursor i wpisywanie tylko w ostatniej linii komendy (np. `terminal_console_input`), bez mozliwosci ustawienia kursora w starych liniach.
- **Sztuczny prompt niezgodny z shell**  
  Nie dopisuj stalego `"> "` jako osobnej linii. Linia wpisywania ma byc doklejana bezposrednio do aktualnego promptu procesu (`cmd`/`pwsh`), jesli prompt nie zakonczyl sie nowa linia.

## Checklista po zmianach terminala/edytora
- Klikniecie terminala: pisanie trafia tylko do terminala.
- Klikniecie edytora: pisanie trafia tylko do edytora.
- `Enter` w terminalu uruchamia komende i poprawnie przechodzi do nastepnej linii.
- Build Debug przechodzi bez bledow.
