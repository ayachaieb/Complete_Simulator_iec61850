import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SinusSignalsComponent } from './sinus-signals.component';

describe('SinusSignalsComponent', () => {
  let component: SinusSignalsComponent;
  let fixture: ComponentFixture<SinusSignalsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SinusSignalsComponent]
    })
    .compileComponents();

    fixture = TestBed.createComponent(SinusSignalsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
