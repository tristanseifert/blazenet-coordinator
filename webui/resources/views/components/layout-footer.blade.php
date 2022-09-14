<footer class="footer">
  {{-- version info --}}
  <div class="content has-text-centered">
    <p>
      {!! __('footer.info', ['name' => config('app.name'), 'version' => config('app.version')]) !!}
    </p>
  </div>
  {{-- connection info --}}
  <div class="content has-text-centered user-info">
    <p>{!! __('footer.userInfo', ['ip' => request()->ip()]) !!}</p>
  </div>
</footer>
