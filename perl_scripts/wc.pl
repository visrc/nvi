sub wc {
  my $words;
  $i = $VI::StartLine;
  while ($i <= $VI::StopLine) {
    $_ = VI::GetLine($VI::ScreenId, $i++);
    $words+=split;
  }
  VI::Msg($VI::ScreenId,"$words words");
}
